package com.sandboxvr.wave.vrsdiagnostic;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.util.Log;
import com.htc.vr.sdk.VRActivity;
import com.sandboxvr.wave.CompositorSettings;
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.InputStream;
import java.security.MessageDigest;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import org.json.JSONObject;

/** Isolated startup-policy diagnostic. No mode is changed after Wave starts. */
public final class MainActivity extends VRActivity {
    private static final String TAG = "VrsDiagnostic";
    public static final String EXTRA_MODE = "compositor_vrs";
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Set<String> reportedRuntimePaths = new HashSet<>();
    private String startupIntentMode = "unset";
    private boolean registered;
    private final Runnable poll = new Runnable() {
        @Override public void run() {
            report("periodic");
            handler.postDelayed(this, 5000);
        }
    };

    @Override protected void onCreate(Bundle state) {
        // This must precede loading our native renderer and VRActivity.onCreate.
        String mode = getIntent().getStringExtra(EXTRA_MODE);
        startupIntentMode = mode == null ? "unset" : mode;
        if ("Off".equals(mode)) {
            CompositorSettings.configureAtStartup(CompositorSettings.Mode.Off);
        } else if ("DeviceDefault".equals(mode)) {
            CompositorSettings.configureAtStartup(CompositorSettings.Mode.DeviceDefault);
        } else if (mode != null && !"unset".equals(mode)) {
            throw new IllegalArgumentException("compositor_vrs must be Off, DeviceDefault, or unset");
        }
        Log.i(TAG, "startup-before-wave mode=" + startupIntentMode + " pid=" + Process.myPid()
                + " aarSha256=" + BuildConfig.WAVE_AAR_SHA256
                + " expectedRuntimeSha256=" + BuildConfig.WAVE_RUNTIME_SHA256);
        System.loadLibrary("vrs_diagnostic");
        nativeRegister();
        registered = true;
        super.onCreate(state);
        report("onCreate");
        recordRequestedMode(getIntent());
    }

    @Override protected void onResume() {
        super.onResume();
        if (registered) nativeSetPaused(false);
        report("onResume");
        handler.removeCallbacks(poll);
        handler.postDelayed(poll, 5000);
    }

    @Override protected void onPause() {
        handler.removeCallbacks(poll);
        if (registered) nativeSetPaused(true);
        report("onPause");
        super.onPause();
    }

    @Override protected void onDestroy() {
        handler.removeCallbacks(poll);
        if (registered) nativeStop();
        report("onDestroy");
        super.onDestroy();
    }

    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        // Startup mode is immutable for this process. A new startup needs a fresh process.
        Log.i(TAG, "new-intent currentStartup=" + startupIntentMode
                + " suppliedStartup=" + intent.getStringExtra(EXTRA_MODE)
                + " startupExtraApplied=false");
        recordRequestedMode(intent);
        report("onNewIntent");
    }

    private void recordRequestedMode(Intent intent) {
        String requested = intent.getStringExtra("request_next_mode");
        if (requested == null) return;
        CompositorSettings.Mode mode = CompositorSettings.Mode.valueOf(requested);
        boolean restart = CompositorSettings.setRequestedMode(this, mode);
        Log.i(TAG, "saved-request mode=" + mode + " restartRequired=" + restart);
        report("requestSaved");
    }

    private void report(String reason) {
        try {
            CompositorSettings.Status status = CompositorSettings.getStatus();
            JSONObject out = new JSONObject();
            out.put("reason", reason);
            out.put("pid", Process.myPid());
            out.put("startupIntent", startupIntentMode);
            out.put("requestedMode", String.valueOf(status.requestedMode));
            out.put("startupMode", String.valueOf(status.startupMode));
            out.put("effectiveMode", String.valueOf(status.effectiveMode));
            out.put("constructorCount", status.constructorCount);
            out.put("drawCount", status.drawCount);
            out.put("supportObserved", status.supportObserved);
            out.put("vendorInputValid", status.vendorInputValid);
            out.put("drawPathSelected", status.drawPathSelected);
            out.put("unsupportedCount", status.unsupportedCount);
            out.put("errorFlags", status.errorFlags);
            out.put("restartRequired", status.restartRequired);
            out.put("renderer", registered ? nativeStatus() : "not-registered");
            Log.i(TAG, out.toString());
            reportMappedRuntime();
        } catch (Exception | LinkageError e) {
            Log.e(TAG, "status-report-failed reason=" + reason, e);
        }
    }

    private void reportMappedRuntime() throws Exception {
        Set<String> paths = new HashSet<>();
        try (BufferedReader maps = new BufferedReader(new FileReader("/proc/self/maps"))) {
            String line;
            while ((line = maps.readLine()) != null) {
                if (!line.contains("libwvr_runtime.so")) continue;
                String[] fields = line.trim().split("\\s+", 6);
                if (fields.length == 6 && !fields[5].endsWith(" (deleted)")) paths.add(fields[5]);
            }
        }
        for (String path : paths) {
            if (reportedRuntimePaths.contains(path)) continue;
            String actual = hashRuntime(path);
            Log.i(TAG, "loaded-runtime path=" + path + " sha256=" + actual
                    + " expected=" + BuildConfig.WAVE_RUNTIME_SHA256
                    + " match=" + BuildConfig.WAVE_RUNTIME_SHA256.equals(actual));
            reportedRuntimePaths.add(path);
        }
    }

    private static String hashRuntime(String path) throws Exception {
        int separator = path.indexOf("!/");
        if (separator >= 0) {
            try (ZipFile archive = new ZipFile(path.substring(0, separator))) {
                ZipEntry entry = archive.getEntry(path.substring(separator + 2));
                if (entry == null) throw new IllegalStateException("Mapped runtime missing from " + path);
                try (InputStream in = archive.getInputStream(entry)) { return sha256(in); }
            }
        }
        try (InputStream in = new FileInputStream(path)) { return sha256(in); }
    }

    private static String sha256(InputStream in) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        byte[] buffer = new byte[65536];
        int count;
        while ((count = in.read(buffer)) != -1) digest.update(buffer, 0, count);
        StringBuilder out = new StringBuilder(64);
        for (byte value : digest.digest()) out.append(String.format(Locale.ROOT, "%02x", value & 255));
        return out.toString();
    }

    private static native void nativeRegister();
    private static native void nativeSetPaused(boolean paused);
    private static native void nativeStop();
    private static native String nativeStatus();
}
