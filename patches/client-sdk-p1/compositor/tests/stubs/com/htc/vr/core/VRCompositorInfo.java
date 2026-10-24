package com.htc.vr.core;
import android.os.Bundle;
public final class VRCompositorInfo {
    public static final Companion Companion = new Companion();
    public static final class Companion {
        public VRCompositorInfo from(Bundle info) { return new VRCompositorInfo(); }
    }
}
