package com.htc.vr.sdk;
import androidx.lifecycle.Lifecycle;
import com.htc.vr.core.VRCompositor;
public final class DoubleWideRenderer {
    public static int allocations;
    public final VRCompositor compositor;
    public DoubleWideRenderer(VRCompositor compositor, Lifecycle lifecycle) {
        this.compositor = compositor;
        allocations++;
    }
}
