package com.htc.vr.core;
import android.os.IBinder;
import android.os.Bundle;
import android.os.IInterface;
import android.os.RemoteException;
import android.view.Surface;
public interface IVRCamera extends IInterface {
    Bundle getVRCameraInfo(IBinder token) throws RemoteException;
    float[] getStereoCameraIntrinsic(IBinder token) throws RemoteException;
    void registerImageUpdateSurface(IBinder token, Surface surface) throws RemoteException;
    void unregisterImageUpdateSurface(IBinder token) throws RemoteException;
}
