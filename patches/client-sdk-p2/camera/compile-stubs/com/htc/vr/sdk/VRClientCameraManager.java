package com.htc.vr.sdk;
import android.content.Context;
import com.htc.vr.core.IVRCameraManager;
public abstract class VRClientCameraManager {
    public boolean mCameraStarted;
    public boolean mUserStarted;
    public VRClientCameraManager(Context context, IVRCameraManager manager) {}
    public interface VRCameraCallback { void onFrameAvailable(byte[] data, int width, int height, long timestamp); }
    public VRCameraCallback mCallback;
}
