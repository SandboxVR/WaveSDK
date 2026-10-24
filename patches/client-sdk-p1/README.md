# Client SDK P1 lifecycle fixes

These patches fix the two client-side P1 findings in the Wave 6.2.0 SDK used by
SSVR. The starting artifact is WaveSDK commit
`647da312ad377108e4187a3643bffaba7662b05d`, including its existing 60 ms native
prediction patch. SSVR commit `33466d446d8bcff38d42db3281c33a09e42a8934`
removed unused client components but still included these runtime defects.

## Behavior

* **Surface dispatch and shutdown:** rejected Handler submissions fail promptly;
  synchronous waits use a completion predicate and a maximum five-second wait;
  thrown work wakes its caller and propagates the error. Timeout/interruption
  cancels work that has not started. Closure rejects new work and queues EGL
  cleanup behind accepted immediate work. Repeated shutdown is harmless. Overlay
  shutdown closes submissions before releasing surfaces, attempts remaining
  releases after a failure, and suppresses queued frame callbacks after closure.
* **Compositor loss:** callback dispatch matches the underlying Binder identity,
  ignores stale/duplicate losses, clears the current compositor before notifying
  the listener, and bounds disconnect waiting to two seconds. Availability stays
  asynchronous to avoid a main-thread registration/Binder callback deadlock.
  A disconnect received during renderer initialization is reconciled when that
  already-started initialization returns.
* **SDK renderer policy:** on a matched compositor loss or replacement, the SDK
  listener becomes terminal and invokes its existing `killOnUiThread()` path.
  The app process exits and requires relaunch. This is intentional: the shipped
  renderer has no complete teardown API, so constructing another renderer could
  leak its rendering thread, shared memory, EGL and lifecycle observer. The patch
  never passes null to native renderer registration and performs no potentially
  blocking remote cleanup before exit. Duplicate availability does not allocate
  another renderer. Initialization exceptions retain fatal behavior.

Already-running work cannot safely be forcibly canceled. The calling thread's
deadline does not guarantee completion of a blocked native driver operation.
These fixes remove demonstrated client failure paths; they do not establish the
cause of the reported headset freeze or repair firmware/kernel code.

## Shipped artifacts

* `wvr_client-6.2.0.aar`: only `assets/wavevr/client-module.apk` changes; only its
  `classes.dex` changes. Original constructors, unrelated methods/classes, APK
  resources and all native libraries remain preserved.
* `wvr.common.utils-6.2.0.aar`: only `classes.jar` changes, applying the same
  CustomizeHandler fix for users of the standalone utility artifact.
* Both artifacts' Gradle module sizes/hashes and MD5/SHA-1/SHA-256/SHA-512 sidecars
  are updated. Public method descriptors remain unchanged.

The editable Java files are implementation donors, **not full reconstructed SDK
sources**. `dispatch/graft-manifest.json` and `compositor/patch-spec.json` allowlist
the exact methods, fields and helper classes to package. Compile stubs never enter
the output. `tools/Graft.java` preserves original metadata and unrelated code,
resolves HTC member references against the output/platform, and verifies every
DEX class after writing it using a normalized single-class DEX comparison.

## Build and checks

Requires Python 3.10+, JDK 17+, Android platform 36.1 and build-tools 36.1.0.
`JAVA_HOME` and `ANDROID_HOME` are honored; the scripts also accept explicit paths.
Build dependencies download from Maven Central into ignored `.build/deps` and
must match pinned SHA-256 hashes.

```powershell
python patches/client-sdk-p1/build.py
& patches/client-sdk-p1/dispatch/run-tests.ps1 -PatchedUtilsJar patches/client-sdk-p1/.build/utils-patched.jar
& patches/client-sdk-p1/compositor/run-tests.ps1
python patches/client-sdk-p1/run_android_smoke.py --serial 10.253.1.235:5555
python patches/client-sdk-p1/build.py --apply
python patches/client-sdk-p1/verify.py
```

The default build writes candidates only; `--apply` updates this repository's
Maven files. The Android smoke uses the actual candidate DEX in a temporary shell
`app_process` with ART verification enabled. It exercises dispatch against real
Android handlers and resolves patched classes. It does not install an app, bind
the compositor, load Wave JNI, or exercise real rendering. Temporary device files
are removed. See `VERIFICATION.json` and `ANDROID_SMOKE.json` for artifact hashes
and the bounded device test result. Host suites cover concurrency, cancellation,
exception handling, stale Binder events and prevention of repeated allocation;
Overlay/Compositor service behavior is modeled by stubs there.

Final validation passed: 146 dispatch assertions against the grafted utility JAR,
23 overlay model assertions, 110 compositor assertions, and 15 isolated headset
ART assertions against the published runtime DEX. `TEST_RESULTS.json` ties these
results to the final artifact hashes; archive/Maven verification also passed.

## Consuming in SSVR

After committing this SDK change, update SSVR's pinned WaveSDK revision and refresh
its vendored Maven snapshot before building SSVR. The loader can select a cached
or downloaded client module before the bundled asset, so verify the loaded module
hash against this patch's candidate/published module during rollout. A new AAR
alone does not prove that an already-running or cached runtime uses the fix.
Use the app's established cache/update procedure; this patch does not change its
module-selection policy or alter installed firmware. A full SSVR rendering soak
and controlled compositor-recovery test remain deployment validation work.
