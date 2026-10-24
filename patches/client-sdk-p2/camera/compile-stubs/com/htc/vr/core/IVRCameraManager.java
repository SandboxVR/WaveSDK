package com.htc.vr.core;
import android.os.RemoteException;
public interface IVRCameraManager {
    void unregisterCameraListener(IVRCameraListener listener) throws RemoteException;
}
