package com.htc.vr.sdk.patch.p1;

import android.util.Log;
import androidx.lifecycle.Lifecycle;
import com.htc.vr.core.ClientSdkP1Compositor;
import com.htc.vr.core.VRCompositor;
import com.htc.vr.core.VRCompositorManager;
import com.htc.vr.sdk.DoubleWideRenderer;
import com.htc.vr.sdk.VRActivityDelegate;
import com.htc.vr.sdk.VRAndroidContext;

/** SDK consumer policy. Generic listeners can reconnect; this renderer cannot be safely reused. */
public final class CompositorLifecycle {
    private CompositorLifecycle() {}

    public static void available(VRCompositorManager.Listener listener, VRActivityDelegate delegate,
            VRAndroidContext context, Lifecycle lifecycle, VRCompositor compositor) {
        if (ClientSdkP1Compositor.isTerminal(listener)) return;
        if (lifecycle.getCurrentState() == Lifecycle.State.DESTROYED) {
            ClientSdkP1Compositor.terminate(listener);
            return;
        }
        // No second rendering thread, observer, or shared-memory owner can be created
        // while the existing native renderer owner has no proven teardown operation.
        if (context.getDoubleWideRenderer() != null) {
            unavailable(listener, delegate, compositor);
            return;
        }
        context.setCompositor(compositor);
        context.setDoubleWideRenderer(new DoubleWideRenderer(compositor, lifecycle));
    }

    public static void unavailable(VRCompositorManager.Listener listener,
            VRActivityDelegate delegate, VRCompositor compositor) {
        boolean wasTerminal = ClientSdkP1Compositor.isTerminal(listener);
        ClientSdkP1Compositor.terminate(listener);
        if (wasTerminal) return;
        Log.e("VRCORE_P1", "Compositor lost; exiting SDK process because renderer teardown is unavailable");
        // This callback runs on the main looper. The original implementation uses
        // Activity.runOnUiThread and immediately Process.killProcess on that thread.
        // Do not call nativeSetRenderer(null) (native code dereferences its argument),
        // or remote unregisterOutputSurface (it can block before local state clears).
        delegate.killOnUiThread();
    }
}
