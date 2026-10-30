# Private application compositor settings

This SDK extension lets an application select compositor variable-rate shading
(VRS) at process startup. It is a SandboxVR extension to the patched Wave 6.2.0-u09
artifact, not an HTC API. The supported setting is deliberately limited to
compositor VRS; it is not a general interface for changing undocumented renderer
flags.

`DeviceDefault` preserves the original device configuration and capability
fallback. SDK consumers that never select a policy retain this behavior. `Off`
forces the compositor's startup VRS flag off before shading-rate resource setup.
The original lens distortion, reprojection, tracking, decoding and refresh-rate
paths remain outside the patch. ALVR foveated encoding and Wave texture foveation
are separate controls. A texture-foveation default flag does not establish that
eye textures actually use texture foveation.

## Application use

Call the extension before the first Wave bootstrap, including before a Wave
Activity superclass can initialize its renderer:

```java
import com.sandboxvr.wave.CompositorSettings;

@Override
protected void onCreate(Bundle savedInstanceState) {
    CompositorSettings.applySavedModeAtStartup(
        this, CompositorSettings.Mode.Off); // SSVR product default
    super.onCreate(savedInstanceState);
    // Continue the application's existing Wave bootstrap.
}
```

Other consumers may use `applySavedModeAtStartup(context)`, whose default is
`DeviceDefault`, or explicitly call
`configureAtStartup(CompositorSettings.Mode.DeviceDefault)`.

When a UI or server setting arrives after Wave initialization, save it for the
next process start:

```java
boolean restartRequired = CompositorSettings.setRequestedMode(
    context, CompositorSettings.Mode.DeviceDefault);
CompositorSettings.Status status = CompositorSettings.getStatus();
```

Present the selected mode and a restart requirement. Restart means a new Android
application process; reconnecting a stream or pausing/resuming the Activity does
not change the active policy. The SDK does not automatically terminate the app.
The preference is stored in that application's private data. The process-local
startup handoff does not modify Android system properties, device-global settings
or firmware.

`getStatus()` verifies the loaded runtime's SHA-256 before reading the private
patch observations. It distinguishes the requested mode, active startup mode,
actual compositor observations, pending initialization, unsupported runtime or
hardware fallback, and errors. An enabled observation establishes selection of
the native VRS drawing path, not a measurement of GPU output or image quality.
See `extension/README.md` for the complete status contract.

## Binary scope and reproduction

The input is commit `023dc61446973ca6c7a968ef824262fb74ee4ba9`. Build inputs are
checked by exact SHA-256 and original instruction bytes. The binary patch adds
startup policy and observation code at the OpenGL compositor constructor and
existing shading-rate branches. Each renderer applies the same sealed process
policy. It does not change a live renderer flag.

The artifact adds the private Java API, its JNI helper and consumer shrinker rules
to the client AAR. The rules preserve the names used by JNI in minified applications.
Within the nested client module, only the arm64 `libwvr_runtime.so` changes. The
module DEX containing the P1/P2 lifecycle fixes, all other native libraries and
the 60 ms prediction-cap data remain preserved. Unsupported runtime versions
are reported rather than accessed using unchecked offsets.

Build tools: Python 3.10+, Android platform 36.1, NDK 25.1.8937393, JDK 17+.

```powershell
python patches/compositor-vrs/build.py
python -m pip install --target "$env:TEMP/wave-vrs-test-deps" -r patches/compositor-vrs/requirements-native-tests.txt
$env:PYTHONPATH = "$env:TEMP/wave-vrs-test-deps"
python patches/compositor-vrs/test_native_hook.py --runtime patches/compositor-vrs/.build/original.so --ndk "$env:LOCALAPPDATA/Android/Sdk/ndk/25.1.8937393" -v
& patches/compositor-vrs/extension/tests/run-tests.ps1 -JavaHome 'C:/Program Files/Android/Android Studio/jbr'
python patches/compositor-vrs/build.py --apply
python patches/compositor-vrs/verify.py
& patches/compositor-vrs/diagnostic/build.ps1
python patches/compositor-vrs/diagnostic/verify_apk.py
```

The build starts from the committed input and writes candidates under `.build/`.
`--apply` updates the AAR, Gradle module metadata and all checksum sidecars after
checking for unrelated changes. `VERIFICATION.json` ties sources and output
artifacts to their hashes. Source hashes normalize CRLF to LF; artifact hashes
use exact bytes.

## Integration and validation

The inspected SSVR checkout initializes Wave before receiving server session
settings. Its integration must apply a saved local policy before Wave bootstrap
and persist late server changes for the next process start. It should explicitly
pass `Off` as its product default. After a committed SDK revision is selected,
update SSVR's pin and Maven snapshot together. A cached/downloaded client module
may take priority over the bundled asset; check the loaded identity and status.

The separate diagnostic app is described in `diagnostic/`. It uses its own package
and data, and does not replace production SSVR. Diagnostic startup/renderer tests
are distinct from actual streaming, through-lens image quality and sustained
75 Hz performance. See [VALIDATION.md](VALIDATION.md) for device results and
outstanding coverage; static analysis alone is not device execution evidence.
