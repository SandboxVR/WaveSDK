package com.htc.vr.client;

import android.os.Binder;
import android.os.Bundle;
import android.os.RemoteException;
import android.util.Log;
import com.google.flatbuffers.FlatBufferBuilder;
import com.htc.vr.component.INotifyDeviceProvider;
import com.htc.vr.core.Logger;
import com.htc.vr.core.VRClientProfile;
import com.htc.vr.fbs.RawNotifyDeviceData;
import java.util.concurrent.CompletableFuture;
import kotlin.Unit;
import kotlin.jvm.internal.Intrinsics;

/** Method donor only. The original Kotlin constructor and metadata are retained. */
public final class VRNotifyDeviceProvider implements AutoCloseable {
    private final String TAG;
    private final String PROP_DEBUG;
    private final CompletableFuture<Unit> closed;
    private final CompletableFuture<Unit> managerClosed;
    private final INotifyDeviceProvider mService;
    private final Binder mToken;
    private final VRClientProfile mClientConfig;
    private final VRNotifyDeviceProvider$mDeathRecipient$1 mDeathRecipient;
    private VRNotifyDeviceProvider$Layer mLayer;
    private CompletableFuture<Unit> mStopped;
    private volatile boolean mStarted;
    private int deviceType_;

    // Both are deliberately zero initialized: the shipped constructor is untouched.
    private boolean mNotifyClosed;
    private boolean mNotifyTransition;

    // Compile/host scaffolding, never grafted. Matches the relevant shipped setup.
    public VRNotifyDeviceProvider(INotifyDeviceProvider service,
            CompletableFuture<Unit> closed) throws RemoteException {
        this.mService = service;
        this.closed = closed;
        TAG = "inv_VRNotifyDeviceProvider";
        PROP_DEBUG = "persist.wave.logd";
        mToken = new Binder();
        mClientConfig = new VRClientProfile(6002000, "6.2.0-u09", 1, null, 8, null);
        mStopped = new CompletableFuture<Unit>();
        managerClosed = new CompletableFuture<Unit>();
        mLayer = new VRNotifyDeviceProvider$Layer(this);
        mDeathRecipient = new VRNotifyDeviceProvider$mDeathRecipient$1(this);
        service.asBinder().linkToDeath(mDeathRecipient, 0);
    }

    public final boolean start(int deviceType, int elementSize) {
        final VRNotifyDeviceProvider$Layer attempt;
        final VRNotifyDeviceProvider$Layer previous;
        synchronized (this) {
            if (mNotifyClosed || mNotifyTransition || mStarted) {
                throw new IllegalStateException("Check failed.");
            }
            if (elementSize < 0 || elementSize > 512) {
                Logger.e(TAG + " start() failed for elementSize outside 0..512");
                return false;
            }
            attempt = new VRNotifyDeviceProvider$Layer(this);
            previous = mLayer;
            mLayer = attempt;
            mNotifyTransition = true;
        }

        boolean needsStop = false;
        boolean registrationAttempted = false;
        boolean committed = false;
        try {
            // Retire any residual owner before replacing it, including an unused
            // constructor-created layer. Allocation/map/build failures roll back.
            if (previous != null) previous.close();
            if (!isNotifyAttemptCurrent(attempt)) return false;
            try {
                attempt.createShareMemory(elementSize);
            } catch (IllegalStateException failure) {
                // close may have retired the published attempt before allocation
                // acquired its layer lock. That is a canceled start, not a leak.
                if (!isNotifyAttemptCurrent(attempt)) return false;
                throw failure;
            }
            if (!isNotifyAttemptCurrent(attempt)) return false;

            // An exception can arrive after the remote side accepted start.
            Bundle profile = mClientConfig.toBundle();
            needsStop = true;
            if (!mService.start(mToken, profile)) {
                needsStop = false;
                return false;
            }
            if (!isNotifyAttemptCurrent(attempt)) return false;
            registrationAttempted = true;
            if (!mService.registerClient(mToken, deviceType, attempt,
                    mClientConfig.toBundle())) return false;

            synchronized (this) {
                if (!mNotifyClosed && mLayer == attempt) {
                    mStopped = new CompletableFuture<Unit>();
                    deviceType_ = deviceType;
                    mStarted = true;
                    committed = true;
                }
            }
            return committed;
        } catch (RemoteException failure) {
            Logger.e(TAG + " start() failed", failure);
            return false;
        } finally {
            try {
                if (!committed) {
                    // Free local memory before potentially blocking rollback IPC.
                    try {
                        attempt.close();
                    } finally {
                        rollbackNotifyRemote(attempt, deviceType,
                                registrationAttempted, needsStop);
                    }
                }
            } finally {
                synchronized (this) {
                    if (!committed) {
                        mStarted = false;
                        deviceType_ = 0;
                    }
                    mNotifyTransition = false;
                }
            }
        }
    }

    private boolean isNotifyAttemptCurrent(VRNotifyDeviceProvider$Layer attempt) {
        synchronized (this) {
            return !mNotifyClosed && mLayer == attempt;
        }
    }

    private void rollbackNotifyRemote(VRNotifyDeviceProvider$Layer layer,
            int deviceType, boolean unregister, boolean stop) {
        try {
            if (unregister) {
                try {
                    mService.unregisterClient(mToken, deviceType, layer);
                } catch (RemoteException failure) {
                    Logger.e(TAG + " unregister failed", failure);
                }
            }
        } finally {
            if (stop) {
                try {
                    mService.stop(mToken);
                } catch (RemoteException failure) {
                    Logger.e(TAG + " stop failed", failure);
                }
            }
        }
    }

    public final void stop(int deviceType) {
        final VRNotifyDeviceProvider$Layer layer;
        final int registeredDevice;
        synchronized (this) {
            // Binder death may close the provider ahead of a queued bridge stop.
            if (mNotifyClosed) return;
            if (!mStarted || mNotifyTransition) {
                throw new IllegalStateException("Check failed.");
            }
            if (deviceType != deviceType_) {
                Logger.w(TAG + " stop the deviceType:" + deviceType + "!=" + deviceType_);
            }
            mNotifyTransition = true;
            mStarted = false;
            registeredDevice = deviceType_;
            deviceType_ = 0;
            layer = mLayer;
        }
        try {
            try {
                mService.unregisterClient(mToken, registeredDevice, layer);
            } catch (RemoteException failure) {
                Logger.e(TAG + " unregister failed", failure);
            } finally {
                // Includes unregister throwing; close waits only for local users.
                try {
                    layer.close();
                } finally {
                    rollbackNotifyRemote(layer, registeredDevice, false, true);
                }
            }
        } finally {
            synchronized (this) {
                mNotifyTransition = false;
            }
        }
    }

    public final void updateData(int deviceType, String str) {
        Intrinsics.checkNotNullParameter(str, "str");
        final VRNotifyDeviceProvider$Layer layer;
        synchronized (this) {
            if (!mStarted) throw new IllegalStateException("Check failed.");
            if (deviceType != deviceType_) {
                Logger.e(TAG + " updateData the deviceType:" + deviceType + "!=" + deviceType_);
                return;
            }
            layer = mLayer;
        }
        FlatBufferBuilder builder = new FlatBufferBuilder();
        builder.clear();
        int data = builder.createString((CharSequence) str);
        RawNotifyDeviceData.startRawNotifyDeviceData(builder);
        RawNotifyDeviceData.addTimestamp(builder, System.nanoTime());
        RawNotifyDeviceData.addDataStr(builder, data);
        builder.finish(RawNotifyDeviceData.endRawNotifyDeviceData(builder));
        if ("1".equals(getSystemProperties(PROP_DEBUG))) {
            Log.d(TAG, TAG + " updateData Str(" + data + "):" + str
                    + " remaining:" + builder.dataBuffer().remaining()
                    + ", limit:" + builder.dataBuffer().limit());
        }
        // The layer lock covers beginWrite, buffer copy and endWrite together.
        // A close that won the race simply discards this already-started update.
        layer.writeNotifyData(builder);
    }

    @Override public void close() {
        final VRNotifyDeviceProvider$Layer layer;
        final CompletableFuture<Unit> stopped;
        synchronized (this) {
            if (mNotifyClosed) return;
            mNotifyClosed = true;
            mStarted = false;
            deviceType_ = 0;
            layer = mLayer;
            stopped = mStopped;
        }
        // Completion handlers and Binder work must run outside both owner locks.
        try {
            layer.close();
        } finally {
            try {
                stopped.complete(Unit.INSTANCE);
            } finally {
                mService.asBinder().unlinkToDeath(mDeathRecipient, 0);
            }
        }
    }

    // Compile/host scaffolding: shipped getters/private method remain untouched.
    public INotifyDeviceProvider getMService() { return mService; }
    public CompletableFuture<Unit> getManagerClosed() { return managerClosed; }
    public CompletableFuture<Unit> getClosed() { return closed; }
    private String getSystemProperties(String key) { return ""; }
}
