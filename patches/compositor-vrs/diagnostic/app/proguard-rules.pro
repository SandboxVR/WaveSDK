# Wave's separately loaded client-module.apk calls this shared Java ABI by name.
# These rules match the existing SSVR integration's Wave 6.2.0 reflection contract.
-keep class com.htc.vr.client.abi.** { *; }
-keep class com.htc.vr.sdk.NativeHelper {
    native <methods>;
}

# The diagnostic renderer exports Java_com_sandboxvr_wave_vrsdiagnostic_MainActivity_*.
-keep class com.sandboxvr.wave.vrsdiagnostic.MainActivity {
    native <methods>;
}

# CompositorSettings JNI names are deliberately covered only by the SDK AAR's
# bundled consumer rule, so this variant verifies that downstream contract.
