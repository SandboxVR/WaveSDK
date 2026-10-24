# Remote camera failed-start cleanup

These files are Java 8 **method donors** for the client-module DEX. Compile
`src/` and `compile-stubs/` against Android platform 36.1, then feed the donor DEX
and `graft-manifest.json` to `patches/client-sdk-p1/tools/Graft.java`. The parent
P2 build combines this allowlist with the other P2 groups. Never package compile
stubs or copy the donor `RemoteVRCameraManager` as a replacement class.

## Behavior and ownership

- Start reserves one session, validates camera information, allocates the image
  handler/reader, and commits `mCameraStarted` only after remote registration and
  acceptance of a queued image drain. Failure returns the existing unknown code
  `99`, closes partial local allocations, and attempts one remote unregister if
  registration may have taken effect. Duplicate active start retains code `7`.
- A stop or release during allocation/registration cancels that session. A new
  start returns `99` until the old remote operation has settled, preventing an old
  unregister from removing a new registration that uses the same Binder token.
  Local resources close before a potentially slow or failing remote unregister.
- The new image listener admits only the current, committed session and reader.
  It counts active invocations while calling the **unchanged shipped** image
  conversion listener. Stop defers reader close until the final active invocation
  returns. A callback can stop its own camera without self-join or close while its
  Image is in use. Already queued old callbacks return before touching the reader.
  The post-commit drain covers notifications received during registration; the
  original listener already handles an empty `acquireLatestImage()` result.
- Listener detachment, reader close and handler-thread quit are attempted even
  when another local cleanup step throws. Cleanup is idempotent. The thread uses
  `quitSafely()` and is never joined. Neither Binder IPC nor user frame callbacks
  execute while the new lifecycle state monitor is held.
- Native stop always cancels resources, including when `mUserStarted` is false
  after a failed start. Normal lifecycle stop preserves `mUserStarted` so existing
  pause/resume behavior continues; native stop and terminal release clear it.
- Terminal release wakes service waiters, unregisters the camera listener once,
  and rejects late availability callbacks. Service loss matches Binder identity,
  ignoring a stale loss from an older service. Replacement cancels/closes the old
  local session and retires remote registration on one named daemon worker, keeping
  its reservation until cleanup finishes. Availability callbacks do not perform
  outbound Binder cleanup. An in-flight start owns its own eventual rollback and
  needs no extra worker. Worker creation failure leaves local resources closed and
  the reservation held, so an old unregister cannot race a new start.
- `getIVRCamera()` uses a predicate and a two-second monotonic deadline, restores
  interruption, and returns null on absence/release. Its retained query callers
  are also patched to handle null: image-information failure continues through
  the original public getters' zero defaults, and intrinsic failure returns null.
  Camera-info Bundle parsing and the original image-format conversion are retained.

The original constructors, inherited/native methods, image conversion callback,
and class/member metadata remain in the packaged classes. Added owner fields use
zero initialization. The manifest explicitly uses donor access flags for the two
start/stop methods to remove their old method-wide synchronization; their state
transitions now use the existing `mLock` without holding a monitor across IPC or
callbacks. Public method/native descriptors do not change. Legacy private setup
helpers are preserved but the replaced lifecycle entry points no longer call them.

## Evidence and regression checks

Static evidence is retained under `audits/2026-10-23/memory/sdk/classes/`:

- `com.htc.vr.sdk.RemoteVRCameraManager.txt`: `startCamera` at `0x22aae4` sets
  started before setup; failed setup/registration does not roll back. `setupSurface`
  at `0x22a6d4` allocates a handler thread before the reader. `stopBackgroundThread`
  at `0x22add4` has an unbounded join. Native stop at `0x22af10` returns immediately
  when the user flag is false. The original query methods dereference a camera
  returned by `getIVRCamera` without checking null.
- `com.htc.vr.sdk.RemoteVRCameraManager$1.txt`: `onImageAvailable` at `0x22a220`
  acquires an Image, performs conversion and invokes the callback under
  `ImageReaderSync`, then closes that Image. This implementation remains unchanged.
- `com.htc.vr.sdk.RemoteVRCameraManager$2.txt`: Binder callbacks at `0x22a5f4` and
  `0x22a674` previously assigned/cleared the camera without a release/stale gate.

Run from the repository root:

```powershell
& ./patches/client-sdk-p2/camera/run-tests.ps1
```

The suite compiles the donors against real `android.jar`, then executes them with
explicit host HandlerThread, ImageReader and Binder stand-ins. It covers partial
allocation and registration failures, rejected drain posts, successful retry,
duplicate stop/start, local cleanup errors, cancellation during allocation and
registration, old unregister versus new start, native ownership, lifecycle resume,
frame arrival before commit, own-thread stop, release during a running callback,
deferred reader close, late/stale/replacement Binder events, service wait timeout,
interruption and null-safe query failures. Current result: **455 assertions**.

These are host resource/concurrency regressions, not an Android camera or JNI
execution. The new listener preserves the original image-handler affinity; real
camera delivery and image lifetime still require headset instrumentation. A
blocked remote Binder call is not forcibly interrupted. An already running frame
callback keeps its reader until it returns, so a callback that never returns can
retain its resources. No lifecycle lock is held while waiting for either case.

## DoubleWideRenderer boundary

The retained `DoubleWideRenderer` constructor allocates a rendering thread,
shared-memory/pose buffers and a lifecycle observer. Its private `stop` only
unregisters the compositor layer; its observer supplies pause/resume, with no full
destroy method. That is a real process-lifetime retention boundary, established
by the saved class disassembly, rather than observed repeated runtime leakage.

P1 makes compositor loss/replacement terminal and uses the SDK's existing process
exit path. The existing SDK destroy path also exits. Within those paths there is
no surviving repeated-renderer allocation trigger to fix independently. A reusable
renderer teardown API becomes actionable if a future supported path disposes an
SDK owner or recovers constructor failure while keeping the same process alive.
This patch does not reconstruct graphics teardown without that demonstrated path.
