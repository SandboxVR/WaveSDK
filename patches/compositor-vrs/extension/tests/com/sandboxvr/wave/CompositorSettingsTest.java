package com.sandboxvr.wave;
import android.content.Context;
import android.content.SharedPreferences;
import java.util.HashMap;
import java.util.Map;
import static com.sandboxvr.wave.CompositorSettings.*;

public class CompositorSettingsTest {
    static class Prefs implements SharedPreferences, SharedPreferences.Editor {
        final Map<String, String> data = new HashMap<>();
        boolean succeeds = true;
        public String getString(String key, String fallback) { return data.containsKey(key) ? data.get(key) : fallback; }
        public Editor edit() { return this; }
        public Editor putString(String key, String value) { data.put(key, value); return this; }
        public boolean commit() { return succeeds; }
    }
    static class Ctx extends Context {
        final Prefs prefs = new Prefs();
        public SharedPreferences getSharedPreferences(String name, int mode) { return prefs; }
    }
    static class Fake implements Bridge {
        boolean loaded;
        boolean valid = true;
        int configured;
        long[] state = new long[13];
        public boolean available() { return true; }
        public String runtimePath() { return loaded ? "/runtime" : null; }
        public boolean matches(String path) { return valid; }
        public int configure(int mode, String path) {
            if ((state[0] & 0x80000000L) != 0) return 1;
            configured = mode;
            return 0;
        }
        public long[] snapshot(String path) { return state.clone(); }
        void constructed(int mode, boolean effective) { loaded = true; state[0] = 0x80000000L | mode; state[1]++; state[4] = effective ? 1 : 0; state[10] = 1; state[11] = 0; state[12] = 0; }
        void draw(boolean enabled) { state[2]++; state[6] = enabled ? 1 : 0; state[11] = 1; }
    }
    static void check(boolean value, String message) { if (!value) throw new AssertionError(message); }
    public static void main(String[] args) throws Exception {
        java.lang.reflect.Method checkHash = CompositorSettings.class.getDeclaredMethod("matchesPackagedRuntime", String.class);
        checkHash.setAccessible(true);
        java.io.File file = java.io.File.createTempFile("wave-vrs-runtime-", ".so");
        file.deleteOnExit();
        try (java.io.FileOutputStream out = new java.io.FileOutputStream(file)) { out.write(new byte[]{97, 98, 99}); }
        check((boolean) checkHash.invoke(null, file.getPath()), "exact runtime file identity");
        try (java.io.FileOutputStream out = new java.io.FileOutputStream(file)) { out.write(new byte[]{97, 98, 99, 100}); }
        check(!(boolean) checkHash.invoke(null, file.getPath()), "changed runtime rejected");
        java.io.File apk = java.io.File.createTempFile("wave-vrs-runtime-", ".apk");
        apk.deleteOnExit();
        try (java.util.zip.ZipOutputStream zip = new java.util.zip.ZipOutputStream(new java.io.FileOutputStream(apk))) {
            zip.putNextEntry(new java.util.zip.ZipEntry("lib/arm64-v8a/libwvr_runtime.so"));
            zip.write(new byte[]{97, 98, 99}); zip.closeEntry();
        }
        check((boolean) checkHash.invoke(null, apk.getPath() + "!/lib/arm64-v8a/libwvr_runtime.so"), "APK entry identity");
        check(!(boolean) checkHash.invoke(null, apk.getPath() + "!/missing.so"), "missing APK entry rejected");
        Ctx context = new Ctx(); Fake runtime = new Fake(); setBridgeForTest(runtime);
        context.prefs.data.put("requested_mode_v1", "invalid");
        applySavedModeAtStartup(context, Mode.Off);
        check(runtime.configured == 1, "invalid preference falls back to consumer default");
        setBridgeForTest(runtime = new Fake());
        context.prefs.data.clear();
        check(getStatus().effectiveMode == Effect.Pending, "unloaded default must be pending");
        applySavedModeAtStartup(context);
        check(runtime.configured == 2 && getStatus().startupMode == Mode.DeviceDefault, "SDK default");
        context.prefs.succeeds = false;
        try { setRequestedMode(context, Mode.Off); throw new AssertionError("failed commit must throw"); }
        catch (IllegalStateException expected) {}
        context.prefs.succeeds = true;
        check(setRequestedMode(context, Mode.Off), "late request needs restart");
        check(getStatus().restartRequired && runtime.configured == 2, "save cannot change current mode");
        configureAtStartup(Mode.Off);
        check(getStatus().restartRequired && runtime.configured == 2, "late configuration reports restart without a live change");
        runtime.constructed(2, true);
        check(getStatus().effectiveMode == Effect.Pending, "constructor is not an enabled draw");
        runtime.draw(true);
        check(getStatus().effectiveMode == Effect.Enabled && getStatus().drawPathSelected, "observed draw");
        configureAtStartup(Mode.Off);
        check(getStatus().restartRequired && runtime.configured == 2, "late configuration reports restart without a live change");
        runtime.constructed(2, true);
        check(getStatus().effectiveMode == Effect.Pending, "old renderer draw is not current");
        runtime.state[10] = 0; runtime.state[3] = 1; runtime.state[11] = 0;
        check(getStatus().effectiveMode == Effect.Pending && !getStatus().vendorInputValid,
              "unstable publication does not expose stale observations");
        setBridgeForTest(runtime = new Fake());
        applySavedModeAtStartup(context, Mode.DeviceDefault);
        check(runtime.configured == 1, "saved request used after restart");
        runtime.constructed(1, false);
        check(getStatus().effectiveMode == Effect.Off && !getStatus().restartRequired, "off after constructor");
        setBridgeForTest(runtime = new Fake());
        runtime.constructed(0, false); runtime.draw(false);
        Status defaultStatus = getStatus();
        check(defaultStatus.startupMode == Mode.DeviceDefault && defaultStatus.effectiveMode == Effect.DeviceDefaultOff, "unset policy uses device default");
        check(setRequestedMode(context, Mode.Off), "persisting after default startup requires restart");
        check(getStatus().startupMode == Mode.DeviceDefault && getStatus().restartRequired, "late persistence preserves actual mode");
        setBridgeForTest(runtime = new Fake());
        runtime.constructed(2, true); runtime.draw(true);
        configureAtStartup(Mode.Off);
        check(getStatus().startupMode == Mode.DeviceDefault && getStatus().effectiveMode == Effect.Enabled && getStatus().restartRequired, "sealed policy remains observable");
        runtime.valid = false;
        check(getStatus().effectiveMode == Effect.Unsupported, "identity failure");
        runtime.valid = true; runtime.state[9] = 1;
        check(getStatus().effectiveMode == Effect.Error, "invalid environment");
        setBridgeForTest(runtime = new Fake());
        runtime.loaded = true; runtime.valid = false;
        check(setRequestedMode(context, Mode.Off), "unknown loaded runtime conservatively requires restart");
        check(getStatus().restartRequired && getStatus().effectiveMode == Effect.Unsupported,
              "status retains conservative restart when identity is unavailable");
        runtime.valid = true; runtime.constructed(1, false);
        check(!getStatus().restartRequired && getStatus().startupMode == Mode.Off,
              "an observed matching startup resolves the conservative restart");
        setBridgeForTest(null);
        System.out.println("CompositorSettings tests passed");
    }
}
