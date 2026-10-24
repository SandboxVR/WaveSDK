package com.htc.vr.sdk;

/** At most one worker per replaced committed session; never joined. */
final class CameraRetirement implements Runnable {
    private final RemoteVRCameraManager owner;
    private final CameraSession session;
    CameraRetirement(RemoteVRCameraManager owner, CameraSession session) {
        this.owner = owner;
        this.session = session;
    }
    public void run() { owner.patchFinishSession(session); }
}
