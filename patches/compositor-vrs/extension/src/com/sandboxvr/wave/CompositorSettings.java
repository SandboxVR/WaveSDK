package com.sandboxvr.wave;

import android.content.Context;
import android.content.SharedPreferences;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.security.MessageDigest;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/** Process-start policy for the patched Wave compositor. Call before Wave bootstrap. */
public final class CompositorSettings {
    public enum Mode { Off, DeviceDefault }
    public enum Effect { Off, Enabled, DeviceDefaultOff, Pending, Unsupported, Error }
    public static final class Status {
        public final Mode requestedMode;
        public final Mode startupMode;
        public final Effect effectiveMode;
        public final long constructorCount;
        public final long drawCount;
        public final boolean supportObserved;
        public final boolean vendorInputValid;
        public final boolean drawPathSelected;
        public final long unsupportedCount;
        public final long errorFlags;
        public final boolean restartRequired;
        private Status(Mode requested, Mode startup, Effect effect, long constructors,
                       long draws, boolean inputValid, boolean drawSelected, long unsupported, long errors, boolean restart) {
            requestedMode = requested; startupMode = startup; effectiveMode = effect;
            constructorCount = constructors; drawCount = draws;
            // This records a selected native draw branch, not a GPU or driver measurement.
            supportObserved = drawSelected; vendorInputValid = inputValid; drawPathSelected = drawSelected;
            unsupportedCount = unsupported; errorFlags = errors; restartRequired = restart;
        }
    }
    private static final String PREFS = "ssvr_wave_compositor_vrs";
    private static final String KEY = "requested_mode_v1";
    private static final Object LOCK = new Object();
    private static Mode startupMode;
    private static Mode requestedMode;
    private static Effect configurationError;
    private static boolean conservativeRestart;
    private static boolean nativeLoaded;
    private static String verifiedPath;
    private static long verifiedLength, verifiedModified;
    interface Bridge {
        boolean available();
        String runtimePath();
        boolean matches(String path);
        int configure(int mode, String path);
        long[] snapshot(String path);
    }
    private static final Bridge REAL_BRIDGE = new Bridge() {
        public boolean available() { return ensureNative(); }
        public String runtimePath() { return nativeRuntimePath(); }
        public boolean matches(String path) { return matchesPackagedRuntime(path); }
        public int configure(int mode, String path) { return nativeConfigure(mode, path); }
        public long[] snapshot(String path) { return nativeSnapshot(path); }
    };
    private static Bridge bridge = REAL_BRIDGE;
    static void setBridgeForTest(Bridge replacement) {
        synchronized (LOCK) {
            bridge = replacement == null ? REAL_BRIDGE : replacement;
            startupMode = null; requestedMode = null; configurationError = null; conservativeRestart = false;
        }
    }
    private CompositorSettings() {}

    /**
     * Save the desired mode for the next process start, without changing the live compositor.
     * Returns true when the current process was configured differently and must be restarted.
     * Throws if Android cannot durably save the preference.
     */
    public static boolean setRequestedMode(Context context, Mode mode) {
        if (context == null || mode == null) throw new IllegalArgumentException("null argument");
        synchronized (LOCK) {
            if (!preferences(context).edit().putString(KEY, mode.name()).commit()) {
                throw new IllegalStateException("Unable to persist compositor mode");
            }
            getStatus(); // Discover an already sealed default when possible.
            requestedMode = mode;
            if (startupMode != null) {
                conservativeRestart = false;
                return startupMode != mode;
            }
            // Preserve the conservative result for getStatus until actual startup is observed.
            try { conservativeRestart = !bridge.available() || bridge.runtimePath() != null; }
            catch (RuntimeException | LinkageError e) { conservativeRestart = true; }
            return conservativeRestart;
        }
    }

    /** Apply a persisted mode, falling back to the SDK default DeviceDefault. */
    public static void applySavedModeAtStartup(Context context) {
        applySavedModeAtStartup(context, Mode.DeviceDefault);
    }

    /** Call before Wave bootstrap; SSVR may pass Off as its product default. */
    public static void applySavedModeAtStartup(Context context, Mode consumerDefault) {
        if (context == null || consumerDefault == null) throw new IllegalArgumentException("null argument");
        synchronized (LOCK) {
            String stored = preferences(context).getString(KEY, null);
            Mode mode = parseMode(stored, consumerDefault);
            requestedMode = mode;
            configureLocked(mode);
        }
    }

    /** Set the current process startup mode. Never changes a sealed compositor policy. */
    public static void configureAtStartup(Mode mode) {
        if (mode == null) throw new IllegalArgumentException("mode");
        synchronized (LOCK) {
            requestedMode = mode;
            configureLocked(mode);
        }
    }

    private static void configureLocked(Mode mode) {
        if (startupMode != null) return; // Keep the request visible; current policy is immutable.

        if (!bridge.available()) { configurationError = Effect.Unsupported; conservativeRestart = true; return; }
        try {
            // If already loaded, validate the exact mapped file before the helper touches its state.
            String path = bridge.runtimePath();
            if (path != null && !bridge.matches(path)) {
                configurationError = Effect.Unsupported;
                conservativeRestart = true;
                return;
            }
            int result = bridge.configure(mode == Mode.Off ? 1 : 2, path);
            if (result == 0) { startupMode = mode; configurationError = null; conservativeRestart = false; }
            else if (result == 1) { configurationError = Effect.Error; conservativeRestart = true; }
            else if (result == 2 || result == 4) { configurationError = Effect.Unsupported; conservativeRestart = true; }
            else { configurationError = Effect.Error; conservativeRestart = true; }
        } catch (RuntimeException | LinkageError e) {
            configurationError = Effect.Error;
            conservativeRestart = true;
        }
    }

    /** Snapshot actual hook observations; Pending means no compositor observation yet. */
    public static Status getStatus() {
        synchronized (LOCK) {
            Mode requested = requestedMode != null ? requestedMode : Mode.DeviceDefault;
            boolean restart = startupMode != null ? requested != startupMode : conservativeRestart;
            if (!bridge.available()) return new Status(requested, startupMode, Effect.Unsupported, 0, 0, false, false, 0, 0, restart);
            try {
                String path = bridge.runtimePath();
                if (path == null) return new Status(requested, startupMode, configurationError != null ? configurationError : Effect.Pending, 0, 0, false, false, 0, 0, restart);
                if (!bridge.matches(path)) return new Status(requested, startupMode, Effect.Unsupported, 0, 0, false, false, 0, 0, restart);
                long[] observation = bridge.snapshot(path);
                if (observation == null || observation.length != 13)
                    return new Status(requested, startupMode, Effect.Unsupported, 0, 0, false, false, 0, 0, restart);
                // Each field is atomically read; concurrent hook updates can make this a mixed-time snapshot.
                long policy = observation[0], constructors = observation[1], draws = observation[2];
                boolean currentRenderer = observation[10] != 0;
                boolean inputValid = currentRenderer && observation[3] != 0;
                boolean currentDraw = observation[11] != 0;
                boolean currentUnsupported = observation[12] != 0;
                boolean drawSelected = currentDraw && draws != 0 && observation[6] != 0;
                if (constructors != 0 && currentRenderer && (policy & 0x80000000L) != 0) {
                    startupMode = (policy & 3) == 1 ? Mode.Off : Mode.DeviceDefault;
                    conservativeRestart = false;
                    restart = requested != startupMode;
                }
                long unsupported = observation[5], errors = observation[9];
                Effect effect;
                if (errors != 0 || (policy & 3) == 3) effect = Effect.Error;
                else if (constructors == 0 || !currentRenderer || (policy & 0x80000000L) == 0) effect = configurationError != null ? configurationError : Effect.Pending;
                else if ((policy & 3) == 1 && observation[4] == 0) effect = Effect.Off;
                else if (currentUnsupported) effect = Effect.Unsupported;
                else if (!currentDraw || draws == 0) effect = Effect.Pending;
                else effect = observation[6] != 0 ? Effect.Enabled : Effect.DeviceDefaultOff;
                return new Status(requested, startupMode, effect, constructors, draws, inputValid, drawSelected, unsupported, errors, restart);
            } catch (RuntimeException | LinkageError e) {
                return new Status(requested, startupMode, Effect.Error, 0, 0, false, false, 0, 0, restart);
            }
        }
    }

    private static SharedPreferences preferences(Context context) {
        return context.getApplicationContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }
    private static Mode parseMode(String value, Mode fallback) {
        if (Mode.Off.name().equals(value)) return Mode.Off;
        if (Mode.DeviceDefault.name().equals(value)) return Mode.DeviceDefault;
        return fallback;
    }
    private static boolean ensureNative() {
        if (nativeLoaded) return true;
        try { System.loadLibrary("ssvr_wave_compositor"); nativeLoaded = true; return true; }
        catch (LinkageError | SecurityException e) { return false; }
    }
    private static boolean matchesPackagedRuntime(String path) {
        String expected = PatchedRuntimeIdentity.SHA256;
        if (expected == null || !expected.matches("[0-9a-f]{64}")) return false;
        try {
            int separator = path.indexOf("!/");
            File source = new File(separator >= 0 ? path.substring(0, separator) : path);
            long length = source.length(), modified = source.lastModified();
            if (path.equals(verifiedPath) && length == verifiedLength && modified == verifiedModified && length > 0) return true;
            boolean matches;
            if (separator >= 0) {
                try (ZipFile apk = new ZipFile(path.substring(0, separator))) {
                    ZipEntry entry = apk.getEntry(path.substring(separator + 2));
                    if (entry == null) return false;
                    try (InputStream in = apk.getInputStream(entry)) { matches = expected.equals(sha256(in)); }
                }
            } else {
                try (InputStream in = new FileInputStream(path)) { matches = expected.equals(sha256(in)); }
            }
            if (matches && source.length() == length && source.lastModified() == modified) {
                verifiedPath = path; verifiedLength = length; verifiedModified = modified;
                return true;
            }
            return false;
        } catch (Exception e) { return false; }
    }
    private static String sha256(InputStream in) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        byte[] buffer = new byte[16384];
        int count;
        while ((count = in.read(buffer)) != -1) digest.update(buffer, 0, count);
        StringBuilder out = new StringBuilder(64);
        for (byte b : digest.digest()) out.append(String.format(Locale.ROOT, "%02x", b & 255));
        return out.toString();
    }
    private static native String nativeRuntimePath();
    // 0 accepted, 1 sealed, 2 unsupported, 3 error, 4 runtime appeared during configuration.
    private static native int nativeConfigure(int mode, String validatedPath);
    private static native long[] nativeSnapshot(String validatedPath);
}
