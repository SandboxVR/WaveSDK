package com.htc.vr.sdk;

import android.media.ImageReader;

final class CameraImageListener implements ImageReader.OnImageAvailableListener, Runnable {
    private final RemoteVRCameraManager owner;
    private final CameraSession session;
    private final ImageReader reader;

    CameraImageListener(RemoteVRCameraManager owner, CameraSession session, ImageReader reader) {
        this.owner = owner;
        this.session = session;
        this.reader = reader;
    }

    public void onImageAvailable(ImageReader reader) {
        owner.patchImageAvailable(session, reader);
    }

    public void run() { onImageAvailable(reader); }
}
