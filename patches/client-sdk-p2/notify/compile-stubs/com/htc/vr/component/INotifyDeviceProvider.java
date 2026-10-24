package com.htc.vr.component;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import com.htc.vr.core.IVRNotifyDeviceClient;
/** Compile-only API surface; no stub class is grafted. */
public interface INotifyDeviceProvider {
    IBinder asBinder();
    boolean start(IBinder token, Bundle profile) throws RemoteException;
    boolean registerClient(IBinder token, int device, IVRNotifyDeviceClient client,
            Bundle profile) throws RemoteException;
    void unregisterClient(IBinder token, int device, IVRNotifyDeviceClient client)
            throws RemoteException;
    void stop(IBinder token) throws RemoteException;
}
