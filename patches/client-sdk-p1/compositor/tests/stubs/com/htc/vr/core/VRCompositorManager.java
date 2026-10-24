package com.htc.vr.core;
import android.os.Handler;
import android.os.Looper;
public final class VRCompositorManager {
    public abstract static class Listener {
        private VRCompositor mCompositor;
        private final Handler mHandler = new Handler(Looper.getMainLooper());
        private final IVRCompositorListener.Stub binder = new VRCompositorManager$Listener$binder$1(this);
        public static Handler access$getMHandler$p(Listener owner) { return owner.mHandler; }
        public static VRCompositor access$getMCompositor$p(Listener owner) { return owner.mCompositor; }
        public static void access$setMCompositor$p(Listener owner, VRCompositor compositor) { owner.mCompositor = compositor; }
        public IVRCompositorListener.Stub getBinder$common_core_release() { return binder; }
        public abstract void onCompositorAvailable(VRCompositor compositor);
        public abstract void onCompositorUnavailable(VRCompositor compositor);
    }
}
