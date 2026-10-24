package com.htc.vr.sdk;
import com.htc.vr.core.VRCompositor;
public final class VRAndroidContext {
    private VRCompositor compositor;
    private DoubleWideRenderer renderer;
    public VRCompositor getCompositor() { return compositor; }
    public DoubleWideRenderer getDoubleWideRenderer() { return renderer; }
    public void setCompositor(VRCompositor compositor) { this.compositor = compositor; }
    public void setDoubleWideRenderer(DoubleWideRenderer renderer) {
        if (renderer == null) throw new AssertionError("unsafe native null renderer");
        this.renderer = renderer;
    }
}
