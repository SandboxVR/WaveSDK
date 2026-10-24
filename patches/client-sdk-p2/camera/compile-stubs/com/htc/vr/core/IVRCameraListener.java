package com.htc.vr.core;
import android.os.Binder;
public interface IVRCameraListener {
    void onCameraAvailable(IVRCamera camera);
    void onCameraUnavailable(IVRCamera camera);
    abstract class Stub extends Binder implements IVRCameraListener {}
}
