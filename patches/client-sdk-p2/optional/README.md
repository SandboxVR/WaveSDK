# Optional client lifecycle fixes

These method donors patch `wvr_ime_client:6.2.0` and `wvr_permission_client:6.2.0`. Each group's explicit `graft-manifest.json` is the packaging allowlist. The original classes, constructors, public signatures, annotations, nest metadata, and unlisted methods remain in the shipped `classes.jar`. New fields use JVM default initialization; constructor replacement is unnecessary. The permission compile-only `R` class and all host Android substitutes are excluded from the shipped artifacts.

The implementation follows the retained shipped-artifact bytecode in:

- `audits/2026-10-23/sdk/wvr_ime_client-6.2.0-classes.javap.txt`: initialization retry, manager/service connections and death recipients, keyboard callback routes, static Context ownership, teardown.
- `audits/2026-10-23/sdk/wvr_permission_client-6.2.0-classes.javap.txt`: Activity registration, config initialization, notification and permission requests, USB retries/receiver, completion, release.

## Build and test

From the repository root:

```powershell
python patches/client-sdk-p2/build.py --group optional/ime --group optional/permission
& patches/client-sdk-p2/optional/run-tests.ps1 `
  -PatchedImeJar patches/client-sdk-p2/.build/optional/ime/patched.jar `
  -PatchedPermissionJar patches/client-sdk-p2/.build/optional/permission/patched.jar `
  -UtilsJar patches/client-sdk-p2/.build/original/utils.jar
```

The parent build pins the P1 baseline and compiles these donors with `javac --release 8` against Android 36.1 `android.jar`, the corresponding original optional JAR, and the original utilities JAR. Utilities supplies the permission/config AIDL interfaces. `Graft jar <manifest> <original.jar> <patched.jar> <donor.jar>` then copies only allowlisted members and helper classes. The parent build owns AAR replacement, Maven metadata, and checksums; the host runner never publishes artifacts.

The host runner requires JDK 17+ and actual grafted JARs. Its classpath contains host Android substitutes, the two grafted JARs, and utilities; donor owner classes are absent. It runs `java -Xverify:all`. Latest focused result: **216 assertions passed**.

Coverage includes normal IME/VR/2D/USB paths, return codes and null callbacks, no-editor fallback, repeat IME submissions and restore, explicit default-route completion, malformed/reordered permission replies, overlapping permission requests, current request identity, stale lifecycle/service callbacks, delayed callback cancellation, bind/registration acceptance races, cross-thread synchronous Binder callbacks, reentrant application teardown, remote registration rollback, linked-death cleanup, bounded retry exhaustion/restart, config and notification work, and USB OS response correlation and setup failure cleanup.

These are deterministic host regressions against shipped Java code and its real AIDL classes. They do not establish device Binder transport, OEM service interoperability, Android broadcast export policy, platform permission UI behavior, or retained heap sizes. An external call already admitted before close can finish against its captured old service/Activity. Local lifecycle references are detached before cleanup, and its return/callback cannot restore ownership in a replacement lifecycle.
