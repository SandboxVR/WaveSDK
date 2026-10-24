package com.htc.vr.core.lifecycle;
import androidx.lifecycle.Lifecycle;
public final class ManualLifecycleOwner {
    private final Lifecycle lifecycle = new Lifecycle();
    public Lifecycle getLifecycle() { return lifecycle; }
}
