# Surface dispatch and OverlaySurfaces P1 repair

These Java files are **method donors**, not replacement SDK source trees. The
build must graft only `graft-manifest.json` members into the original packaged
classes. Constructors, EGL context creation/configuration, the original EGL
anonymous helpers, surface allocation/release implementations, native calls,
and public descriptors stay in place. `compile-stubs/` and the donor's nested
`OverlaySurface` are compilation aids and must never enter the resulting SDK.

## Behavior

- All CustomizeHandler `run*` entry points serialize queue acceptance with a
  closed gate. Rejected posts throw `RejectedExecutionException` immediately.
  Inherited final Android `Handler.post*` methods retain platform behavior.
- Synchronous dispatch waits on a completion predicate and a monotonic deadline.
  The no-timeout overload and `(0,0)` use five seconds; a positive requested
  timeout uses the shorter of that duration and five seconds. Invalid durations
  fail before dispatch, and large durations cannot overflow conversion.
- `SyncRunnable` executes outside its completion monitor, records Throwable,
  and always marks completion and calls `notifyAll` in `finally`. RuntimeException
  and Error instances propagate to the waiting caller. A failure arriving after
  the caller's deadline/interruption is logged on the worker.
- A timed-out or interrupted work item that has not started is marked canceled
  under its monitor and removed from the Android queue. Interrupt status is
  restored. Started work cannot safely be stopped; it may finish after its caller
  receives an `IllegalStateException` with a timeout/interruption cause.
- Quit closes acceptance first, enqueues EGL cleanup behind previously accepted
  immediate work, and quits the looper in `finally`. Same-thread quit returns to
  the queue so it cannot overtake accepted callbacks. A foreign-thread quit waits
  up to five seconds; its cleanup remains queued after timeout/interruption.
  Repeated quit returns. Closing a handler for the shared main looper does not
  attempt Android's forbidden main-looper quit.
- OverlaySurfaces uses a new volatile `mClosing` flag. Existing synchronized
  add/remove reject after closing; quit closes before its checked asynchronous
  post. The handler reference remains available while an accepted add finishes
  constructing SurfaceTexture listeners. Cleanup tries each surface, clears the
  list and shuts down the handler even if a release throws. Asynchronous failures
  are logged. A rejected cleanup post reports failure; it cannot perform EGL
  release on a dead looper or move that release onto the caller thread.
- The original frame callback's first synthetic enable getter now also checks
  `mClosing`. Already queued callbacks therefore return before native/texture
  access after shutdown. Callbacks that passed the gate before closing finish on
  the same queue before surface cleanup runs. Setting frame enable again cannot
  reopen a closed overlay.

The original synchronized Overlay add/remove methods retain their access flags.
Their existing worker callbacks do not acquire the owner monitor. CustomizeHandler
holds its lifecycle monitor only for acceptance and never while running user
work or waiting for completion.

## Build contract

Compile `src/` plus `compile-stubs/` with JDK `javac --release 8 -Xlint:-options`
against an Android platform `android.jar`. The manifest preserves original
method flags/annotations when replacing a body and uses donor field flags for
new fields. The two Overlay donor names are mapped onto the original synthetic
method names; their bodies do not recursively reference those donor names.

The Java 8 compiler creates `CustomizeHandler.access$000`; it is explicitly
included in the manifest for the new `ShutdownRunnable`. Neither constructor in
the donor output is included. No generated donor class should be copied merely
because it was emitted by javac/D8; the manifest is the allowlist.

## Host regression checks

From the repository root in PowerShell:

```powershell
./patches/client-sdk-p1/dispatch/run-tests.ps1
./patches/client-sdk-p1/dispatch/run-tests.ps1 -PatchedUtilsJar '<path-to-grafted-classes.jar>'
```

The script compiles against the local Android SDK and runs the same compiled
donor methods against explicit host Android queue/EGL stubs. Supplying a patched
utility JAR puts the actual grafted CustomizeHandler classes before donor classes
on the runtime classpath; the test prints the loaded class origin. Overlay tests
execute the donor lifecycle bodies with stand-ins for unchanged GL add/remove
callbacks, and exercise a framework callback queued after surface release.

The checks cover accepted/rejected work, exceptional work, completion races,
spurious notifications, queued cancellation on timeout/interruption, started
work returning after a deadline, detached-error logging, large/invalid duration
handling, finite default and shutdown waits, shutdown cleanup surviving a caller
deadline, owner-thread EGL order, duplicate/racing quit, main-looper closure,
overlay admission versus quit, release-failure continuation and queued frame
callback suppression.

These are host concurrency regressions, not an ART/EGL/headset reproduction.
They do not validate real graphics drivers, native callback latency, a permanently
blocked JNI call, packaged-module selection, or final DEX verification. The
original per-surface release routine can still fail partway through its own
buffers; this narrow repair continues other surfaces and reports that error.
Device instrumentation remains necessary for graphics lifecycle validation.
