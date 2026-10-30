# Compositor VRS validation — 2026-10-30

Validation of the private compositor setting, based on SDK commit
`023dc61446973ca6c7a968ef824262fb74ee4ba9`.

## Exact final artifacts

| Artifact | SHA-256 |
| --- | --- |
| Client AAR | `81b73a4031cc243b6748aeb14f64567150ffc79e04ca44ef0c7ba905ed2297ba` |
| Nested client module | `9606d6f87f13a3203186a9814e9eaed2cb40635b2be20a7269659db83e240c70` |
| ABI 2 arm64 runtime | `14a717297a5724c2dfc8c105896793f0457746079a2106e77a1780bf6e370c63` |
| Debug diagnostic APK | `b5cafe19ba63b32e991f74a322e4b37a5b1f23c24d23e48ac6707b2591c5e9d3` |
| Minified diagnostic APK, tested on device | `921b8331c985b1c3faae9b2a9a30007ca8b52da2c276a95f03e08cfe838b53b4` |

Both APKs use package `com.sandboxvr.wave.vrsdiagnostic` and the separate
diagnostic certificate SHA-256
`aa06fe11994281b9f9d6063cd1e17528cf9d311102152d27a6abab5f30352b5a`.
The minified build is non-debuggable. APK signing verification passed, and R8's
merged configuration and resulting DEX confirmed that the SDK consumer rule
preserved the private JNI class and method signatures.

## Automated checks

- All 16 actual AArch64 instruction tests passed under Unicorn 2.1.4. Coverage
  includes both policies, absent/invalid configuration, original resource/draw
  gates, first-constructor sealing, recreation, register preservation, concurrent
  publication, sequence wrap, unknown inputs and exact byte preservation.
- Host-JVM tests passed for defaults, preferences, failed durable writes, late
  requests, restart reporting, pending/unsupported/error states, recreation and
  stale observations, and runtime-file/APK-entry identity checks.
- Two complete SDK builds produced identical runtime, AAR and module hashes.
  Artifact verification passed for the exact original instructions, unchanged
  module DEX and other libraries, cap60 data, Java additions, consumer rules,
  Maven metadata and all checksum sidecars.
- Debug and minified APK builds passed. `diagnostic/verify_apk.py` confirmed each
  packaged the exact final module/runtime/helper and private API.

## Focus Vision execution

Device: VIVE Focus Vision, Android 12 firmware fingerprint
`HTC/kona/kona:12/SKQ1.230810.001/14690:user/dev-keys`, Adreno 650.
Wave reports client 6.2.0-u09 and server 6.2.0-u25 and emits its existing version
compatibility warning. Runtime initialization and rendering nevertheless succeeded.

| Test | Observed result |
| --- | --- |
| Off | Exact final runtime loaded; effective Off with valid vendor input; VRS draw gate false and no native VRS-enable log; 863 submitted diagnostic frames across pause/resume, zero submit errors. |
| DeviceDefault | Exact final runtime loaded; `Enable shading rate with config: 600,11,800,12,1000,44`; effective Enabled with VRS draw gate true; 989 diagnostic frames across pause/resume, zero submit errors. |
| No configuration API call | Startup policy discovered as DeviceDefault; original configuration log restored; effective Enabled, 83 diagnostic frames and zero submit errors. |
| Late request, both directions | Requested mode changed and `restartRequired=true`; startup/effective policy remained unchanged through pause/resume in the same process. Frame and draw counters increased after resume. |
| Unsupported runtime | Negative APK paired the final API/helper with the exact pre-VRS cap60 runtime (`7d6bcf75…`). Loaded identity mismatched, effective Unsupported, and no private state counters were read. Wave continued rendering with its original configuration. |
| Restore after negative test | Final minified APK reinstalled; exact final runtime loaded again, Off observed, 59 frames and zero submit errors; diagnostic process then stopped. |

The kiosk watchdog's automatic relaunch limited active test windows and supplied
the observed pause/resume transitions. Device ownership, allowlists and firmware
were not changed. No global renderer overrides were used.

Production SSVR was restored to the resumed `alvr.client.SbvrActivity` in `LOCKED`
kiosk mode, with its deployment health service still foreground. The installed
production version is **1.5.4.0.rc46-75Hz**, not historical rc50. Its APK SHA-256
remained `fc61d706ad8c0ce86ac0f3cdad3ccbaaf3312b4f7c81d195c0b27f0c3dd21390`.
It was not reinstalled, uninstalled or data-cleared. The final diagnostic APK
remains installed separately and stopped.

Raw device captures and generated APKs remain local and are excluded from source
control. The diagnostic sources and artifact verification scripts reproduce the
checks above.

## Boundaries and remaining integration

These tests establish loader acceptance, actual cached flags and native draw
branch selection with submitted diagnostic frames. They do **not** establish
through-lens sharpness, a GPU capture of shading rates, real SSVR streaming,
sustained 75 Hz performance, thermals or long-duration kernel stability. Focus 3
was not connected. Hardware without VRS was exercised through the emulated
fallback, not a physical unsupported headset. Renderer destruction/recreation
was tested in emulation; device pause/resume retained one renderer. Applying a
saved preference after a fresh process start was host-tested; the diagnostic's
device runs deliberately use explicit startup extras or no selection.

SSVR integration is separate from this SDK change. Apply the saved policy with
consumer default Off in `SbvrActivity.onCreate`
**before** its Wave superclass/bootstrap. Wave initialization in
`android/app/src/main/cpp/cpp_main.cpp` precedes ALVR initialization and server
stream configuration, so late server settings must be persisted for restart.
Pin the committed SDK revision using `DEFAULT_WAVE_SDK_REVISION` in
`alvr/xtask/src/dependencies.rs` and regenerate the vendor snapshot; do not edit
only generated vendor files. Add the application setting using the API documented
in [README.md](README.md). ALVR encoding and Wave texture foveation remain separate.
