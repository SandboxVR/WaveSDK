# Gaze surface lifecycle fix

This patch addresses PROD 1.5.3.2 Crashlytics issue
`c2e4f4975f60c4a76ab15c3b5e9fd929`: `drawGaze` indexes a null surface array during
`triggerClientReady`. The same unchecked dereference exists in the customized
Wave 6.2.0-u09 module at commit `3d654a98a3855666a58068895d6bb7a02a41f562`.

The original `initGaze` only refreshes its surface array when the overlay ID does
not already exist. The original `releaseOverlaySurfaces` clears that array and
queues surface release without sharing `triggerClientReady`'s monitor. Checking
only for null would leave both stale resources and concurrent release possible.

## Behavior

- Initialization, complete drawing and resource detachment share the decorator's
  existing monitor. `triggerClientReady` already holds that monitor while
  publishing a new overlay owner; constructors and that method remain unchanged.
- Gaze initialization reacquires the current owner's two surfaces even when its
  ID exists. Missing owners, inflater/views, partial arrays, null elements and
  invalid surfaces skip the draw and can recover on a later draw.
- The draw uses an owned two-element snapshot. Stable buffers are not resized
  repeatedly. It also holds the surface owner's monitor so Binder callbacks
  cannot remove/regenerate an individual overlay while it is drawing.
  Double-buffer selection and the original renderer are preserved.
- Release waits for an active draw, detaches all gaze/owner references, resets
  swap state, then calls the retired owner's original asynchronous `quit` outside
  the decorator monitor. A new owner is independent of old cleanup. Repeated
  release is harmless, including after a cleanup exception.
- Rendering/cleanup errors still propagate. This is not a broad exception
  handler and does not promise to cancel a blocked native rendering operation.

## Packaging boundary

`graft-manifest.json` replaces exactly three methods of
`VROverlayServiceManagerDecorator`: `initGaze`, `drawGaze` and
`releaseOverlaySurfaces`. Java files are implementation donors and compile-only
stubs, not complete reconstructed SDK sources. Constructors, fields, unrelated
methods, the original `drawViewOnSurface`, all other classes and every native
library remain preserved. No compile-only class/method is packaged.

Only `assets/wavevr/client-module.apk!/classes.dex` changes inside the client
AAR. The VRS/prediction native patches and P1/P2 lifecycle code are preserved.
The build verifies every DEX class by canonical structural comparison and checks
that every other ZIP member is byte-identical. Maven `.module` hashes/sizes and
all four AAR/module checksum sidecars are updated.

## Rebuild and verify

Requires JDK 17+, Python 3.9+, Android platform 36 and build-tools 36.1.0. Set
`JAVA_HOME` and `ANDROID_HOME`, or supply `--java-home` and `--android-sdk`.
Pinned Maven dependencies are downloaded and SHA-256 checked using the existing
P1 build helper. No global tool/configuration installation is performed.

```sh
python3 patches/client-sdk-gaze/build.py
python3 patches/client-sdk-gaze/run_tests.py
python3 patches/client-sdk-gaze/verify.py
python3 patches/client-sdk-gaze/build.py --apply
```

`--apply` requires a passing regression report for the exact current source and
candidate bytes. It refuses unrelated Maven-file modifications. Candidate
artifacts are in `.build/candidate/repo`; published outputs are the matching
files under `repo/com/htc/vr/wvr_client/6.2.0/`. Reports are `HOST_TESTS.json`
and, after apply, `VERIFICATION.json`.

The host suite grafts the exact compiled donor methods used to produce the DEX
into Android/render test stand-ins. It covers missing and partial resources,
recovery of an existing ID, valid double buffering, stale references, 30
controlled draw/release interleavings, late callbacks, retirement overlapping a
replacement owner, individual overlay removal during drawing, and rendering/cleanup exceptions. It is not headset runtime
validation; device smoke testing should exercise startup, gaze progress, pause,
resume and repeated release/recreation.

An ALVR build pinned to the old remote SDK commit still downloads the old AAR.
Use the verified local artifact for local validation, then pin the commit that
contains these Maven outputs when publication is authorized. No native symbol
upload/build-ID change is needed for this Java-only SDK patch.
