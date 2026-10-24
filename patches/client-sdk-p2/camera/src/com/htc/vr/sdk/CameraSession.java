package com.htc.vr.sdk;

import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;
import com.htc.vr.core.IVRCamera;

/** All mutable fields are protected by the owner's existing mLock. */
final class CameraSession {
    ImageReader reader;
    HandlerThread thread;
    Handler handler;
    IVRCamera camera;
    boolean starting = true;
    boolean cancelled;
    boolean closed;
    boolean registerAttempted;
    boolean unregisterClaimed;
    boolean retirementClaimed;
    int callbacks;
}
