# JNI uses exported Java_com_sandboxvr_wave_CompositorSettings_* symbols.
# Ship this with the AAR so consumers need not supply their own native-name rule.
-keep,includedescriptorclasses class com.sandboxvr.wave.CompositorSettings {
    native <methods>;
}
