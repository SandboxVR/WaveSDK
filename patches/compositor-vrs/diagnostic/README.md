# Wave compositor VRS diagnostic

This standalone app uses `com.sandboxvr.wave.vrsdiagnostic` and a separately generated local debug signing key. It consumes `com.htc.vr:wvr_client:6.2.0` exclusively from this checkout's `repo`. It does not use the production app or modify tracked SDK samples.

The renderer follows the native SDK sample's `WVR_RegisterMain` / `VRActivity` startup, calls `WVR_Init` and `WVR_RenderInit`, and submits two 1024 × 1024 eye textures containing procedural color bars and a checkerboard. There are no streaming, Firebase, Crashlytics, or upload dependencies. This diagnostic verifies runtime policy and rendering activity; it does not verify through-lens appearance, performance, or streaming quality.

Build after the patched SDK AAR and private extension have been installed in the local repository:

```powershell
.\build.ps1
python verify_apk.py
```

The script uses Android Studio's JBR, the installed Android SDK, NDK 25.1.8937393, CMake 3.22.1, and cached Gradle/AGP. It creates `.local/vrs-diagnostic-debug.keystore`, builds offline, and prints APK and SDK hashes. It never installs or launches the app. `-NativeOnly` builds the renderer independently of the extension's Java API. The wrapper is copied from the sibling `alvr_private/android` project; no production Gradle tasks are imported.

The APK is `app/build/outputs/apk/debug/app-debug.apk`; expected SDK hashes are also written to `app/build/sdk-identity.txt` and compiled into the diagnostic. The app records the actual mapped `libwvr_runtime.so` path and SHA-256, and whether it matches the expected runtime from its build's AAR.

For the R8/JNI packaging check, first run `build.ps1` to create the diagnostic signing key and build the debug variant, then build the `minified` variant using the same diagnostic-only key:

```powershell
$env:JAVA_HOME = 'C:/Program Files/Android/Android Studio/jbr'
$env:ANDROID_HOME = "$env:LOCALAPPDATA/Android/Sdk"
.\gradlew.bat --offline --no-daemon :app:assembleMinified
python verify_apk.py --apk app/build/outputs/apk/minified/app-minified.apk
```

This variant disables app/native debugging and runs R8 with Android's default optimization rules and the existing Wave shared-ABI keep rules. The SDK AAR's consumer rule must preserve `com.sandboxvr.wave.CompositorSettings` and its `nativeRuntimePath`, `nativeConfigure`, and `nativeSnapshot` methods; the diagnostic supplies no additional keep rule for that API. Inspect `app/build/outputs/mapping/minified/configuration.txt` for the rule originating from the SDK's `proguard.txt`, and `mapping.txt` for the unchanged declaring class. R8 may omit unchanged native members from its mapping; inspect the APK's DEX definitions to confirm all three method names and signatures remain native. The minified APK has the same diagnostic package ID as the debug APK, so installing one replaces the other diagnostic variant.

To install and launch on the diagnostic headset, use the explicit connected-device serial for every command. These commands target only the diagnostic package:

```powershell
$adb = "$env:LOCALAPPDATA/Android/Sdk/platform-tools/adb.exe"
$device = '10.253.1.235:5555'
$pkg = 'com.sandboxvr.wave.vrsdiagnostic'
& $adb -s $device install -r .\app\build\outputs\apk\debug\app-debug.apk
# Fresh process, no configure call: observe the SDK/environment default.
& $adb -s $device shell am force-stop $pkg
& $adb -s $device shell am start -n "$pkg/.MainActivity"
# Fresh process, explicitly disabled before Wave bootstrap.
& $adb -s $device shell am force-stop $pkg
& $adb -s $device shell am start -n "$pkg/.MainActivity" --es compositor_vrs Off
# Fresh process, explicitly use the device default before Wave bootstrap.
& $adb -s $device shell am force-stop $pkg
& $adb -s $device shell am start -n "$pkg/.MainActivity" --es compositor_vrs DeviceDefault
# Pause/resume without stopping the process (run for each startup mode).
& $adb -s $device shell input keyevent KEYCODE_HOME
& $adb -s $device shell am start -n "$pkg/.MainActivity"
& $adb -s $device logcat -d -v threadtime -s VrsDiagnostic:I VrsDiagnosticNative:I AndroidRuntime:E
```

Allow at least two five-second status intervals in each active stage when operator controls permit. Verify `loaded-runtime ... match=true`, initialization result zero, increasing renderer `frames`, increasing compositor `drawCount`, stable process ID across pause/resume, expected `startupMode` and `effectiveMode`, and no fatal exception. `Pending` during startup does not establish an effective policy. For unset, Java `startupMode` is initially null; after native construction the getter discovers the sealed `DeviceDefault` policy without setting it.

The `compositor_vrs` extra accepts `Off`, `DeviceDefault`, and `unset` (or absence). A new intent in the same process never reconfigures the startup policy. An optional `--es request_next_mode Off` (or `DeviceDefault`) exercises `setRequestedMode` and records `restartRequired` without changing the running compositor. Saved requests are diagnostic-package preferences; this app's startup is always selected explicitly by `compositor_vrs`, so a saved request cannot contaminate the unset test.

The diagnostic does not require replacing the production package, firmware changes, or global settings. A kiosk may block its launch. Use operator controls to exit kiosk mode; a temporary production-process stop must be separately authorized and the original application/kiosk session restored afterward. Some device-owner watchdogs immediately relaunch the production app; report the resulting short test window instead of treating it as a sustained run. Do not remove device ownership or its lock-task allowlist.

When rebuilding the same diagnostic version, confirm the *loaded* runtime hash again. Wave may select a cached module. If necessary, `adb -s <serial> shell pm clear com.sandboxvr.wave.vrsdiagnostic` clears only this test app's data and cache; never clear the production package. The extension reports an unmatched loaded runtime as `Unsupported`.

For a negative runtime-identity test, first build the minified APK, then run
`python build_unsupported_fixture.py`. This creates a test APK signed with the diagnostic key
under `../.build/unsupported-fixture/`, replacing only the diagnostic module's
runtime with the exact pre-VRS cap60 binary. It deliberately fails
`verify_apk.py`; on device its expected status is `Unsupported`, never a claimed
effective Off. The script does not install anything. Restore the final diagnostic
APK afterward and recheck its loaded runtime hash. See `../VALIDATION.md` for
the completed Focus Vision checks and their limits.
