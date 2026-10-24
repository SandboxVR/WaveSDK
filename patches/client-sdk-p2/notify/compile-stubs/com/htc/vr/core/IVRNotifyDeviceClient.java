package com.htc.vr.core;
import android.os.Binder;
import android.os.Bundle;
public interface IVRNotifyDeviceClient {
    Bundle getSharedMemory();
    Bundle call(String method, String arg, Bundle extras);
    void notify(String method, String arg, Bundle extras);
    abstract class Stub extends Binder implements IVRNotifyDeviceClient { }
}
