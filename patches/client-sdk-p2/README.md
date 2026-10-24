# Client SDK P2 resource and lifecycle fixes

This patch continues the client-package audit on `fix/client-sdk-lifecycle`,
starting from P1 commit `210f74320180e9d58135deb4305094618d70eff1`.
It preserves the P1 dispatch/compositor fixes and the native 60 ms prediction cap.

## Scope

* **Notify-device:** roll back local shared memory after failed start/registration,
  clean up even when remote unregister fails, close pending/active memory on
  provider close, and serialize local writes/descriptor duplication against close.
  A fresh layer belongs to each attempt; old callbacks cannot use a new layer or
  freed memory. Terminal stop is harmless, including after Binder death.
* **Remote camera:** commit startup only after setup/registration succeeds; close
  partial ImageReader/thread allocations on failure; clean up failed native starts
  even when the user-started flag is false. Active frame callbacks retain their
  reader until they finish. Service/lifecycle callbacks reject stale ownership.
* **Optional IME:** cancel delayed connection work on destruction, invalidate old
  connection/service callbacks, release binding/death-recipient ownership, clear
  keyboard callback state, and use application-context ownership for service binds.
  Failed handshakes allow at most three automatic retries; accepted pending binds
  wait for callbacks without periodic polling.
* **Optional permission client:** release Activity/request references, service
  bindings, USB receiver and scheduled work; reject replies from old lifecycles
  and requests. Application callbacks and service calls do not run under the new
  lifecycle state locks.

These are packaged client-library fixes. Optional-feature defects do not imply
SSVR currently activates those features. The sample-only native ownership/logging
findings and firmware service/driver behavior are outside this client patch.

The incomplete `DoubleWideRenderer` teardown is covered by P1's terminal process
exit policy on compositor loss/replacement and the existing Activity-destroy exit.
No remaining repeated renderer-allocation trigger was established in those paths.
This patch does not introduce in-process renderer reuse or remove that exit policy.

## Artifacts and reproducibility

The build updates three AARs and their Gradle module/checksum files:

* `wvr_client-6.2.0.aar`: only the nested client module's `classes.dex` changes.
* `wvr_ime_client-6.2.0.aar`: only `classes.jar` changes.
* `wvr_permission_client-6.2.0.aar`: only `classes.jar` changes.

The P1 standalone `wvr.common.utils` artifact is unchanged. Native libraries,
resources, constructors and unlisted methods remain preserved. Editable Java
files are selective method donors, not complete reconstructed SDK sources.
Each group's `graft-manifest.json` is the packaging allowlist; compile-only stubs
never enter the resulting SDK. The retained P1 graft tool verifies the complete
DEX round trip and keeps every non-allowlisted class structurally unchanged.

Requires Python 3.10+, JDK 17+, Android platform 36.1 and build-tools 36.1.0.
`JAVA_HOME`/`ANDROID_HOME` or explicit script options select the installed tools.
Build dependencies are pinned by SHA-256 and reused from the P1 build cache when
available. `build.py` starts from the committed P1 artifacts, never from a partly
patched working artifact, and records source/artifact hashes.
Source hashes normalize CRLF to LF so Git's Windows text checkout does not
invalidate the record; all artifact and Maven checksums hash exact bytes.

```powershell
python patches/client-sdk-p2/build.py
python patches/client-sdk-p2/run_host_tests.py
python patches/client-sdk-p2/run_android_smoke.py --serial 10.253.1.235:5555
python patches/client-sdk-p2/build.py --apply
python patches/client-sdk-p2/verify.py
```

Without `--apply`, artifacts remain in ignored `.build/candidate/`. Partial
`--group` builds support local debugging but cannot be applied to the Maven repo.
The verifier checks every published checksum and constrained archive change.
`HOST_TESTS.json` and `ANDROID_SMOKE.json` record validation against the exact
sources and binaries; the verifier rejects stale validation records.
Historical P1 verification reports describe the prior artifact; use the P2
verifier for the final combined SDK.

## Validation boundary and rollout

Host suites inject allocation, service and lifecycle failures. Notify/camera host
tests model Android/native resources; optional-module tests execute grafted JVM
classes with Android/service stand-ins. The isolated headset smoke resolves the
actual candidate classes under ART and rechecks P1 dispatch. It does not replace
SSVR, bind optional services, or establish real camera/keyboard/permission behavior.
See each group's README and the final verification records for exact coverage.

Final validation passed:

| Check | Result |
| --- | --- |
| Notify-device host lifecycle suite | 1,141 assertions |
| Camera host resource/concurrency suite | 455 assertions |
| Optional clients, actual grafted JARs | 216 assertions |
| Isolated headset ART dispatch smoke | 15 assertions |
| Patched ART class/member resolution | 22 classes |
| Runtime structural preservation | 6,782 classes checked; 7 allowlisted changes/additions |

`VERIFICATION.json` records the applied AARs and all 30 Maven artifact, metadata,
and checksum files. The final runtime DEX SHA-256 is
`7db9c1a759941de217726fff76af2338adaa874db43a207c9b294adc06228d50`.

An already-running callback or blocked Binder operation cannot be forcibly
canceled safely. Local ownership is closed or deferred until its active user
finishes, rather than freeing memory under that user. These changes fix observed
code defects; they do not prove the cause of the reported black-screen freeze.

After committing, update SSVR's WaveSDK pin and vendored Maven snapshot. Verify the
loaded client module hash during rollout: the loader can select cached/downloaded
modules before the bundled asset. Test real feature start/stop, failure recovery
and rendering soak separately. Firmware installation is unchanged by these AARs.
