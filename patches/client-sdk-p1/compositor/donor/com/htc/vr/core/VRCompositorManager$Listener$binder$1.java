package com.htc.vr.core;

import android.os.Bundle;

/** Compile donor only. Graft the two virtual implementations; retain all original metadata. */
public final class VRCompositorManager$Listener$binder$1 extends IVRCompositorListener.Stub {
    final VRCompositorManager.Listener this$0;

    VRCompositorManager$Listener$binder$1(VRCompositorManager.Listener listener) {
        this$0 = listener;
    }

    @Override public void compositorAvailable(IVRCompositor binder, Bundle info) {
        ClientSdkP1Compositor.available(this$0, binder, info);
    }

    @Override public void compositorUnavailable(IVRCompositor binder) {
        ClientSdkP1Compositor.unavailable(this$0, binder);
    }
}
