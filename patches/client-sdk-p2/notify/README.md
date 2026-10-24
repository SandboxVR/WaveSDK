# Notify-device shared-memory lifecycle patch

This donor fixes SDK-M1 from
[`sdk-memory-findings.md`](../../../audits/2026-10-23/memory/sdk-memory-findings.md).
The observed implementation is the shipped Wave 6.2.0-u09 client-module DEX;
the package is `com.htc.vr.client`, not `com.htc.vr.core`. Existing P1 changes
must be preserved when this manifest is grafted onto the current runtime.

Each start attempt owns a fresh Layer. Only a successful service start and client
registration publish `mStarted=true`; false responses, RemoteException and other
exceptions roll back the local allocation. Ambiguous accepted remote operations
receive best-effort unregister/stop cleanup. A failed remote cleanup cannot skip
the local close. Negative sizes and sizes above 512 return false before allocation;
zero remains accepted. Stop unregisters the actual registered device even when
the caller supplies a different type, while retaining the mismatch warning.

Local layer close is terminal and idempotent. Provider close frees initialized or
pending memory, including when reached through the unchanged Binder death callback.
That callback completes `managerClosed` first; an external synchronous completion
handler that never returns can still delay its call to close. Pending start attempts
cannot commit after provider close. A transition flag rejects overlapping
start/stop calls, and no Binder IPC runs with the provider or layer monitor held.
Close can therefore reclaim local memory while a remote start/register/unregister
call is blocked. A layer monitor protects native map/build, descriptor duplication,
complete ring writes and destruction. A delayed update keeps its original layer
and is discarded if that layer has closed, including after a successful restart.

The original constructor, Kotlin metadata, death callback, FlatBuffer copy lambda,
unrelated methods and all other classes are retained. The manifest copies four
provider methods, three layer methods, three zero-initialized boolean fields and
three explicit helper methods. `use_donor_access` removes the old declared
synchronization from start/stop; their donor bodies use short explicit lock
sections. No helper class or compile stub is packaged. The donor constructors,
getters, private system-property placeholder and layer callback placeholders are
compile/host scaffolding and are excluded by the member allowlist.

`mStopped` retains its observed contract: initialized in the constructor, replaced
after successful registration, completed by close. Original stop does not complete
it. The external `closed` future is not completed here; Binder death still completes
`managerClosed` before calling close. Close performs local teardown and the existing
death-recipient unlink; remote start/stop cleanup stays with those operations.
Stop after terminal close is harmless, so Binder death cannot make a queued bridge
stop fail. The normal stop-before-start/failed-start guard still throws. The shipped
NotifyDeviceModule.close posts stop and then close on the same Handler before
quitSafely/join; host checks cover that order and death ahead of the queued stop.

## File-descriptor and callback ownership

The original `SharedMemory.toBundle` calls `ParcelFileDescriptor.fromFd` at DEX
`0x204e62`, then puts that object in the returned Bundle. Android documents
[`fromFd`](https://developer.android.com/reference/android/os/ParcelFileDescriptor#fromFd(int))
as creating an independently owned duplicate. The layer monitor keeps the native
owner alive through this duplication. The returned Bundle can then be serialized
after local native close without referring to the destroyed native handle.

The unchanged `IVRNotifyDeviceClient$Stub.onTransact` calls getSharedMemory at
`0x1f4824`, checks null at `0x1f4832`, and writes nonnull results to the reply at
`0x1f483c` using flags=1. A retired callback returning null is supported by that
shipped Stub. The patch introduces no additional Bundle exports or retained Bundle
references. Returned duplicate PFDs remain subject to the existing Android
Binder/ParcelFileDescriptor ownership lifecycle. The layer does not prematurely
close a callback's duplicate, because reply serialization happens after callback
return. Nested Bundle propagation of return-value flags is framework-dependent;
this patch does not assume it closes every nested PFD immediately. Android's PFD
[finalization path](https://android.googlesource.com/platform/frameworks/base/+/5c9d0de174caf151937f19aa6898f60bce3fb4f4/core/java/android/os/ParcelFileDescriptor.java)
also closes an otherwise unclosed owned descriptor. Host tests model independent
duplication; they do not execute Android Binder marshalling or measure FD reclamation.

Evidence is in the audit's `memory/sdk/classes/com.htc.vr.client.VRNotifyDeviceProvider*` class dumps,
`com.htc.vr.core.jni.SharedMemory.txt` and full `runtime.dexdump.txt`. The original
runtime has only one Layer.getWriter caller, the replaced provider.updateData.
The unchanged raw getter is not used by any patched memory access path. The native
bridge's cached false result still bounds repeated native failed starts; the retry
test deliberately calls the public Java provider directly.

## Build and host verification

Requires JDK 17+, Android platform 36.1 and build-tools 36.1.0. From repository root:

```powershell
& patches/client-sdk-p2/notify/compile-donor.ps1
& patches/client-sdk-p2/notify/run-tests.ps1
```

Both scripts accept `-Jdk`; donor compilation also accepts `-AndroidSdk`. The exact
production source set is `src/**/*.java` plus `compile-stubs/**/*.java`, compiled
with `javac --release 8 -encoding UTF-8 -cp <android.jar>`. D8 uses `--release
--min-api 29 --lib <android.jar>` on that compiled donor JAR. The donor DEX is
`out/donor/dex/classes.dex`; pass it and `graft-manifest.json` to the existing
`patches/client-sdk-p1/tools/Graft.java` merge tool. All D8 inputs, including stubs,
are compile-time inputs only; the exact member manifest is the packaging allowlist.
The standalone compile script does not edit any Maven artifact or target DEX.

The host suite runs these same donor methods against instrumented ownership,
Android and service models. It passed **1,141 assertions**, covering 100 direct
failed retries, create/map/build/parse/writer failures, start/register/unregister/
stop failures, runtime exceptions, idempotent unused/active close, actual-device
unregistration, Binder death, stale callbacks, descriptor-duplication versus close,
write versus close, close before allocation, close during blocked start/register/
unregister, overlapping lifecycle requests, and an old update delayed across restart.
The fake FlatBuffer builder tests lifecycle and payload-copy invocation; it does
not test FlatBuffer encoding or native ring implementation. Actual output DEX
reference/link validation, archive preservation and device behavior require the
integrating build's checks. No device has been installed, started or changed by
this host suite.
