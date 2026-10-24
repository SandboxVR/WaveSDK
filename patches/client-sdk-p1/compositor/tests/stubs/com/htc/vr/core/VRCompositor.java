package com.htc.vr.core;
public final class VRCompositor {
    public final IVRCompositor binder;
    public VRCompositor(IVRCompositor binder, VRCompositorInfo info) { this.binder = binder; }
}
