# Compositor callback P1 repair

This patch preserves the original SDK class definitions and replaces four method
implementations. It adds five helper classes. `patch-spec.json` is the exact graft
allowlist; no constructors, annotations, fields, interfaces, or original method
access flags are replaced.

## Resulting behavior

- Generic `VRCompositorManager.Listener` instances receive ordered callbacks on
  their existing main-looper Handler. Matching loss clears `mCompositor` before
  calling the consumer. A subsequent connection can reuse the same listener.
- Duplicate availability for the same `IVRCompositor.asBinder()` token does not
  construct a new wrapper or call the consumer again. Loss for an older Binder
  cannot tear down the current compositor. An active replacement calls the old
  owner's loss callback before delivering the new owner.
- Availability remains asynchronous, so a service callback during a main-thread
  `registerListener` call cannot wait on that blocked main thread. Undelivered
  obsolete availability is coalesced; matching loss cancels undelivered availability.
- Disconnect called on the main looper drains inline. Other callers wait at most
  two seconds. Rejected/throwing `Handler.post`, interruption, and timeout cancel
  pending work. Interrupted callers regain their interrupt flag.
- A timed-out queued disconnect never later mutates state or calls a consumer.
  A callback already executing cannot be safely aborted. If matching loss arrives
  while availability initialization is executing, that started callback retires
  its invalidated owner when initialization returns, even if the queued loss was
  canceled. This compensation is part of completing the started callback.
- A reentrant callback queues its next event behind the current callback instead
  of waiting on its own stack. Availability callback exceptions remain fatal on
  the main looper after making the listener terminal. Disconnect exceptions are
  reported to its waiting caller; the listener stays terminal to avoid creating
  another resource owner after uncertain cleanup.
- The registry uses weak listener identity and weak active Binder references.
  A local Binder that captures its listener cannot create a permanent registry
  retention cycle. Pending events retain their consumer only while queued/running.

**The SDK Activity consumer deliberately exits on matched compositor loss.** Its
`DoubleWideRenderer` has no proven complete teardown path. Enabling a second
renderer would add another rendering thread, lifecycle observer, and shared-memory
owner. The delegate callback marks dispatch terminal and clears listener ownership
before calling its existing `killOnUiThread` implementation, which kills the SDK
process on the main thread. This is crash prevention with controlled process exit,
not seamless SDK rendering recovery. Generic consumers that implement their own
teardown can reconnect on the same listener.

The SDK path never passes null to `VRAndroidContext.setDoubleWideRenderer` and
does not issue synchronous remote output-surface unregister before process exit.
It also prevents a late callback from constructing a renderer after the Activity
lifecycle reaches `DESTROYED`. If the process-exit call returns in a test or altered
runtime, terminal dispatch still prevents a second renderer.

## Donor compilation and grafting

Compile `src/**/*.java`, `donor/**/*.java`, `tests/stubs/com/**/*.java`, and
`tests/stubs/androidx/**/*.java` against the real Android platform `android.jar`
using `javac --release 11`. **Never include `tests/stubs/android` in a production
build or package any compile-only stub class.** These stubs express only the
existing signatures needed to compile donors, not replacement SDK implementations.

D8 must process both donor `VRActivityDelegate.class` and its anonymous
`VRActivityDelegate$1.class` together. Java 11 nest attributes make D8 emit calls
to the original outer class's accessors:

```
Lcom/htc/vr/sdk/VRActivityDelegate;->-$$Nest$fgetmVRContext:(Lcom/htc/vr/sdk/VRActivityDelegate;)Lcom/htc/vr/sdk/VRAndroidContext;
Lcom/htc/vr/sdk/VRActivityDelegate;->-$$Nest$fgetmLifecycleOwner:(Lcom/htc/vr/sdk/VRActivityDelegate;)Lcom/htc/vr/core/lifecycle/ManualLifecycleOwner;
```

Graft only the four allowlisted method bodies and add the allowlisted helper
classes. Keep the original `VRActivityDelegate` outer class and its accessors.
Validate every HTC method/field reference against the resulting DEX; fail the
build if D8 emits different accessor names or any stub implementation is selected.
The binder donor references the original public synthetic listener accessors.
The original synthetic lambda methods/classes remain as unused ABI-preserved code.

## Host verification

From the repository root:

```powershell
& './patches/client-sdk-p1/compositor/run-tests.ps1'
```

The script compiles the actual helpers and donor classes with deterministic host
Android/SDK stubs. It checks asynchronous initial availability; Binder-token
deduplication; disconnect/reconnect/replacement order; stale loss; pending event
coalescing; same-looper and reentrant loss; rejected and throwing posts; queued and
executing callback timeouts; no late queued loss mutation; invalidated in-flight
initialization compensation; interrupted wait cancellation; callback failure
semantics; terminal-before-exit SDK behavior; destroyed lifecycle; no repeated
renderer allocation; and collection of completed/connected weak owners, including
a local Binder capturing its listener. Background-thread exceptions fail the suite.

Latest local result: **110 assertions passed** on Android Studio JBR, Java 11
target, Windows host. These are helper control-flow tests, not Android Binder,
ART verifier, JNI renderer teardown, GPU, or headset execution tests. They do not
establish seamless compositor recovery or safe full renderer destruction.

## Original evidence

All addresses refer to the original SDK DEX retained at
`audits/2026-10-23/memory/sdk/runtime.dexdump.txt` and its selected class files:

- Listener loss lambda `0x2014f4`: calls loss without clearing `mCompositor`.
- Listener availability lambda `0x20158c`: throws if `mCompositor` is non-null.
- Listener loss Binder method `0x20167c`: ignores Binder identity and post result,
  then waits with unbounded `FutureTask.get`.
- Delegate available callback `0x2333b8`: always constructs `DoubleWideRenderer`.
- Delegate unavailable callback `0x233434`: only logs.
- Renderer constructor `0x226d1c`: creates rendering thread/shared memory and adds
  a lifecycle observer. Its observer only starts/stops layer registration.
- Delegate `onDestroy` `0x2347e8` and `killOnUiThread` `0x23444c`: existing process
  termination policy. `VRActivityDelegate$4.run` at `0x2339f4` calls `killProcess`.
- Context `setCompositor` `0x23679c`: performs synchronous remote unregister
  before writing the field. `setDoubleWideRenderer` `0x23687c` calls native code;
  independent native inspection established that a null renderer is unsafe.
