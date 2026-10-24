package com.htc.vr.sdk;

import com.htc.vr.core.VRCompositor;
import com.htc.vr.core.VRCompositorManager;
import com.htc.vr.core.lifecycle.ManualLifecycleOwner;
import com.htc.vr.sdk.patch.p1.CompositorLifecycle;

/**
 * Compile donor only. Compile with Java 11+ nest attributes, then D8 both this
 * outer class and its anonymous inner class together. D8 emits the existing
 * -$$Nest$fgetmVRContext / -$$Nest$fgetmLifecycleOwner accessors. Graft ONLY the
 * two callbacks in VRActivityDelegate$1; the original outer class remains intact.
 */
public class VRActivityDelegate {
    private VRAndroidContext mVRContext;
    private ManualLifecycleOwner mLifecycleOwner;
    private final VRCompositorManager.Listener mCompositorListener = new VRCompositorManager.Listener() {
        @Override public void onCompositorAvailable(VRCompositor compositor) {
            CompositorLifecycle.available(this, VRActivityDelegate.this, mVRContext,
                    mLifecycleOwner.getLifecycle(), compositor);
        }

        @Override public void onCompositorUnavailable(VRCompositor compositor) {
            CompositorLifecycle.unavailable(this, VRActivityDelegate.this, compositor);
        }
    };

    // Signature-only placeholder. The original method performs process termination.
    public void killOnUiThread() {}
}
