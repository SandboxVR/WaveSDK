package com.htc.vr.sdk;

import com.htc.vr.core.IVRCamera;
import com.htc.vr.core.IVRCameraListener;

/** Method donor only: retain the packaged constructor and synthetic field. */
class RemoteVRCameraManager$2 extends IVRCameraListener.Stub {
    final RemoteVRCameraManager this$0;

    RemoteVRCameraManager$2(RemoteVRCameraManager owner) { this$0 = owner; }

    public void onCameraAvailable(IVRCamera camera) {
        this$0.patchCameraAvailable(camera);
    }

    public void onCameraUnavailable(IVRCamera camera) {
        this$0.patchCameraUnavailable(camera);
    }
}
