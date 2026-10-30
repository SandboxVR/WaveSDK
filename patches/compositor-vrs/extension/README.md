# Wave compositor VRS startup extension

This private Java/JNI API selects the patched Wave compositor policy. It is separate
from ALVR foveated encoding, Wave texture foveation, and game-side VRS. It does not
install or change any headset system property. The patched Wave runtime and this
helper must be packaged together; an unpatched runtime is reported as unsupported.

Before any Wave bootstrap or renderer construction:

```java
// SDK default when no saved value exists: DeviceDefault.
CompositorSettings.applySavedModeAtStartup(context);
// Or a product may choose its own default, e.g. SSVR:
CompositorSettings.applySavedModeAtStartup(context, CompositorSettings.Mode.Off);
```

For a one-process startup request, call `configureAtStartup(Mode.Off)` or
`configureAtStartup(Mode.DeviceDefault)` at the same point. `setRequestedMode`
commits a private preference for the **next process start**, does not change the
live compositor, and returns whether a restart is required. If startup has not
been configured and the runtime is already loaded, the result conservatively
requires a restart. After a process restart, apply the saved mode before bootstrap.
Invalid saved values fall back to the supplied default. The process-wide API is
synchronized; calling configure for a different mode after it has been configured
leaves the live policy unchanged and reports `restartRequired`. Calling after Wave bootstrap is unsupported even if a particular call
happens to be accepted before renderer construction.

`getStatus()` reports requested and startup modes, an effective observation,
constructor and draw counts, input-valid/draw-path observations, unsupported and
error counts, and `restartRequired`. `Pending` means the relevant runtime or
observation has not arrived. `Off` means the constructor's compositor VRS flag was
observed cleared. `Enabled` means an enabled native draw branch was observed, not
that a GPU capture verified the shading rate. `DeviceDefaultOff` means the
observed device-default draw did not select that branch. `supportObserved` is
only a selected native draw-branch observation: it does not prove GPU/driver
support. `Unsupported` includes binary identity/ABI mismatch and observed runtime
unsupported handling. `Error` includes malformed startup environment. Counts
are aggregate process diagnostics. Observation fields are read under the runtime's
writer sequence; overlapping publication yields Pending rather than attributing
an old renderer's draw to the latest renderer. The startup policy is also checked
before and after the snapshot. Activity or renderer recreation does
not reset the process startup policy. Restart means a fresh OS process, not only
an activity recreation or reconnect.

The package build supplies a generated package-private
`com.sandboxvr.wave.PatchedRuntimeIdentity` class with a lowercase 64-character
`static final String SHA256` for the exact patched `libwvr_runtime.so`. The helper
library is `libssvr_wave_compositor.so`, placed in the AAR's `jni/arm64-v8a/`.
The AAR also includes consumer shrinker rules to preserve JNI entry-point names.
Before accessing state, Java hashes the mapped runtime file (or the runtime entry
inside an APK), caches a verified path/file-size/mtime identity, and JNI checks
the loaded pathname, ELF segment permissions, and private header ABI 2 on every
access. All ELF header and state accesses happen inside `dl_iterate_phdr` callbacks,
while Android bionic holds its recursive dynamic-linker mutex. The callback makes a
nested inventory to confirm uniqueness under that same lock; it does not call into
Java or keep a reference after the callback. Bionic's implementation of the shared
mutex and its use for both iteration and `dlclose` is documented in
[Android bionic's linker source](https://android.googlesource.com/platform/bionic/+/refs/heads/main/linker/dlfcn.cpp).
An unreadable, mismatched, ambiguous, or unpatched runtime is unsupported.
The native hook consumes `SSVR_WAVE_COMPOSITOR_VRS_V1` only at the first renderer
construction, atomically sealing its policy; a request is not runtime acceptance.
No renderer object is dereferenced by this extension.

`tests/` provides small host-JVM Android stubs and a fake native bridge for policy
and persistence tests. For example, compile the extension source and all test Java
sources with Java 8 or newer and run `com.sandboxvr.wave.CompositorSettingsTest`.
The tests do not substitute for a headset test of the patched runtime and GL path.
