package com.htc.vr.sdk;

import android.content.Context;
import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import com.htc.vr.core.DeprecatedCameraInfo;
import com.htc.vr.core.IVRCamera;
import com.htc.vr.core.IVRCameraListener;
import com.htc.vr.core.IVRCameraManager;

/** Method/field donor. The constructor and declaration-only methods are excluded. */
public class RemoteVRCameraManager extends VRClientCameraManager {
    private Context mContext;
    private IBinder mBinder;
    private Object mLock;
    private IVRCamera mIVRCamera;
    private IVRCameraManager mIVRCameraManager;
    private DeprecatedCameraInfo mDeprecatedCameraInfo;
    private ImageReader mImageReader;
    private HandlerThread mBackgroundThread;
    private Handler mBackgroundHandler;
    public IVRCameraListener mListener;
    private ImageReader.OnImageAvailableListener mOnImageAvailableListener;

    // Deliberately zero-initialized: the original constructor is preserved.
    private CameraSession mP2CameraSession;
    private boolean mP2CameraReleased;

    public RemoteVRCameraManager(Context context, IVRCameraManager manager) {
        super(context, manager);
    }

    private DeprecatedCameraInfo getVRCameraInfo() {
        if (mContext.checkSelfPermission("android.permission.CAMERA") != 0) return null;
        synchronized (mLock) {
            if (mP2CameraReleased) return null;
            if (mDeprecatedCameraInfo != null) return mDeprecatedCameraInfo;
        }
        IVRCamera camera = getIVRCamera();
        if (camera == null) return null;
        try {
            DeprecatedCameraInfo info = DeprecatedCameraInfo.Companion.from(camera.getVRCameraInfo(mBinder));
            synchronized (mLock) {
                if (mP2CameraReleased || mIVRCamera != camera) return null;
                mDeprecatedCameraInfo = info;
                return info;
            }
        } catch (RemoteException | RuntimeException failure) {
            Log.w("RemoteVRCameraManager", "getVRCameraInfo failed", failure);
            return null;
        }
    }

    public float[] getCameraIntrinsic() {
        IVRCamera camera = getIVRCamera();
        if (camera == null) return null;
        try {
            return camera.getStereoCameraIntrinsic(mBinder);
        } catch (RemoteException | RuntimeException failure) {
            Log.w("RemoteVRCameraManager", "getCameraIntrinsic failed", failure);
            return null;
        }
    }

    public IVRCamera getIVRCamera() {
        long deadline = System.nanoTime() + 2000000000L;
        synchronized (mLock) {
            while (mIVRCamera == null && !mP2CameraReleased) {
                long remaining = deadline - System.nanoTime();
                if (remaining <= 0) return null;
                try {
                    mLock.wait(remaining / 1000000L, (int) (remaining % 1000000L));
                } catch (InterruptedException interrupted) {
                    Thread.currentThread().interrupt();
                    return null;
                }
            }
            return mP2CameraReleased ? null : mIVRCamera;
        }
    }

    public int startCamera() {
        CameraSession session;
        synchronized (mLock) {
            if (mP2CameraReleased) return 99;
            if (mP2CameraSession != null) return mCameraStarted && !mP2CameraSession.starting ? 7 : 99;
            session = new CameraSession();
            mP2CameraSession = session;
        }
        HandlerThread thread = null;
        Handler handler = null;
        ImageReader reader = null;
        boolean committed = false;
        try {
            IVRCamera camera = getIVRCamera();
            if (camera == null) return 99;
            DeprecatedCameraInfo info = getVRCameraInfo();
            if (info == null || info.getImgWidth() <= 0 || info.getImgHeight() <= 0) return 99;
            synchronized (mLock) {
                if (session.cancelled || mP2CameraReleased) return 99;
            }
            thread = new HandlerThread("CameraBackgroundForImageReader");
            thread.start();
            handler = new Handler(thread.getLooper());
            reader = ImageReader.newInstance(info.getImgWidth(), info.getImgHeight(), 35, 2);
            synchronized (mLock) {
                session.thread = thread;
                session.handler = handler;
                session.reader = reader;
                session.camera = camera;
                if (session.cancelled || mP2CameraReleased || mIVRCamera != camera) return 99;
                mBackgroundThread = thread;
                mBackgroundHandler = handler;
                mImageReader = reader;
            }
            CameraImageListener listener = new CameraImageListener(this, session, reader);
            reader.setOnImageAvailableListener(listener, handler);
            synchronized (mLock) {
                if (session.cancelled || mP2CameraReleased) return 99;
                session.registerAttempted = true;
            }
            camera.registerImageUpdateSurface(mBinder, reader.getSurface());
            synchronized (mLock) {
                if (session.cancelled || mP2CameraReleased || mIVRCamera != camera) return 99;
                // Queue acceptance is local and cannot invoke the listener inline. Keep it
                // atomic with commit so a queued notification cannot precede the result.
                // The original callback tolerates acquireLatestImage() returning null.
                if (!handler.post(listener)) throw new IllegalStateException("camera drain post rejected");
                session.starting = false;
                mCameraStarted = true;
                committed = true;
                return 0;
            }
        } catch (RemoteException | RuntimeException failure) {
            Log.e("RemoteVRCameraManager", "startCamera failed; rolling back", failure);
            return 99;
        } finally {
            if (!committed) {
                synchronized (mLock) {
                    // Capture even a partial allocation that failed before publication.
                    if (!session.closed) {
                        session.thread = thread;
                        session.handler = handler;
                        session.reader = reader;
                    }
                    session.starting = false;
                    session.cancelled = true;
                    mCameraStarted = false;
                    patchDetachSession(session);
                }
                patchFinishSession(session);
            }
        }
    }

    public boolean startCameraFromNative() {
        if (mContext.checkSelfPermission("android.permission.CAMERA") != 0) return false;
        synchronized (mLock) {
            if (mP2CameraReleased) return false;
            if (mUserStarted) return true;
        }
        int result = startCamera();
        synchronized (mLock) {
            if (result == 0 && mCameraStarted && !mP2CameraReleased) {
                mUserStarted = true;
                return true;
            }
            return false;
        }
    }

    public void stopCamera() {
        CameraSession session;
        synchronized (mLock) {
            session = mP2CameraSession;
            mCameraStarted = false;
            if (session == null) return;
            session.cancelled = true;
            patchDetachSession(session);
        }
        patchFinishSession(session);
    }

    public void stopCameraFromNative() {
        synchronized (mLock) { mUserStarted = false; }
        // Failed starts also require cleanup; mUserStarted is not an ownership gate.
        stopCamera();
    }

    public void releaseCameraResource() {
        synchronized (mLock) {
            if (mP2CameraReleased) return;
            mP2CameraReleased = true;
            mUserStarted = false;
            mIVRCamera = null;
            mLock.notifyAll();
        }
        try {
            stopCamera();
        } finally {
            try {
                mIVRCameraManager.unregisterCameraListener(mListener);
            } catch (RemoteException | RuntimeException failure) {
                Log.w("RemoteVRCameraManager", "unregisterCameraListener failed", failure);
            }
        }
    }

    // Caller holds mLock. The current session remains reserved until remote cleanup ends.
    private void patchDetachSession(CameraSession session) {
        if (mP2CameraSession != session) return;
        mImageReader = null;
        mBackgroundHandler = null;
        mBackgroundThread = null;
    }

    void patchFinishSession(CameraSession session) {
        // Local resources close before a possibly failing or slow Binder operation.
        try {
            patchCloseSession(session);
        } finally {
            IVRCamera camera = null;
            boolean claimed = false;
            synchronized (mLock) {
                if (!session.starting && !session.unregisterClaimed) {
                    session.unregisterClaimed = true;
                    claimed = true;
                    if (session.registerAttempted) camera = session.camera;
                }
            }
            if (claimed) {
                try {
                    if (camera != null) camera.unregisterImageUpdateSurface(mBinder);
                } catch (RemoteException | RuntimeException failure) {
                    Log.w("RemoteVRCameraManager", "unregisterImageUpdateSurface failed", failure);
                } finally {
                    synchronized (mLock) {
                        if (mP2CameraSession == session) mP2CameraSession = null;
                    }
                }
            }
        }
    }

    private void patchCloseSession(CameraSession session) {
        ImageReader reader;
        HandlerThread thread;
        synchronized (mLock) {
            if (!session.cancelled || session.callbacks != 0 || session.closed
                    || (session.starting && session.reader == null && session.thread == null)) return;
            session.closed = true;
            reader = session.reader;
            thread = session.thread;
            session.reader = null;
            session.thread = null;
            session.handler = null;
        }
        try {
            if (reader != null) {
                try {
                    reader.setOnImageAvailableListener(null, null);
                } catch (RuntimeException failure) {
                    Log.w("RemoteVRCameraManager", "detach image listener failed", failure);
                } finally {
                    reader.close();
                }
            }
        } catch (RuntimeException failure) {
            Log.w("RemoteVRCameraManager", "close image reader failed", failure);
        } finally {
            if (thread != null) {
                // Never join: this path may run from that very thread or a reentrant callback.
                try { thread.quitSafely(); }
                catch (RuntimeException failure) {
                    Log.w("RemoteVRCameraManager", "quit camera thread failed", failure);
                }
            }
        }
    }

    void patchImageAvailable(CameraSession session, ImageReader reader) {
        synchronized (mLock) {
            if (mP2CameraReleased || mP2CameraSession != session || session.cancelled
                    || session.starting || !mCameraStarted || session.reader != reader) return;
            session.callbacks++;
        }
        try {
            // Preserve the shipped conversion, timestamps, callback and ImageReaderSync monitor.
            mOnImageAvailableListener.onImageAvailable(reader);
        } finally {
            synchronized (mLock) { session.callbacks--; }
            patchCloseSession(session);
        }
    }

    void patchCameraAvailable(IVRCamera camera) {
        if (camera == null) return;
        CameraSession replaced = null;
        boolean retire = false;
        synchronized (mLock) {
            if (mP2CameraReleased) return;
            if (mIVRCamera != null && mIVRCamera.asBinder().equals(camera.asBinder())) {
                // A new AIDL wrapper for the current Binder is a duplicate event.
                mLock.notifyAll();
                return;
            }
            if (mIVRCamera != null && !mIVRCamera.asBinder().equals(camera.asBinder())) {
                replaced = mP2CameraSession;
                if (replaced != null) {
                    replaced.cancelled = true;
                    mCameraStarted = false;
                    patchDetachSession(replaced);
                    if (!replaced.starting && !replaced.retirementClaimed && !replaced.unregisterClaimed) {
                        replaced.retirementClaimed = true;
                        retire = true;
                    }
                }
            }
            mIVRCamera = camera;
            mLock.notifyAll();
        }
        if (replaced != null) patchCloseSession(replaced);
        if (retire) {
            try {
                Thread retirement = new Thread(new CameraRetirement(this, replaced), "RemoteVRCameraRetirement");
                retirement.setDaemon(true);
                retirement.start();
            } catch (RuntimeException | OutOfMemoryError failure) {
                // Local resources already closed. Keep the reservation so a later
                // unregister cannot race a new registration with the same token.
                Log.w("RemoteVRCameraManager", "camera retirement thread rejected", failure);
            }
        }
    }

    void patchCameraUnavailable(IVRCamera camera) {
        CameraSession session;
        synchronized (mLock) {
            if (mP2CameraReleased || camera == null || mIVRCamera == null
                    || !mIVRCamera.asBinder().equals(camera.asBinder())) return;
            mIVRCamera = null;
            mCameraStarted = false;
            session = mP2CameraSession;
            if (session != null) {
                session.cancelled = true;
                patchDetachSession(session);
                // A disconnected service no longer owns a usable registration.
                if (!session.starting && !session.retirementClaimed && !session.unregisterClaimed) {
                    session.unregisterClaimed = true;
                    mP2CameraSession = null;
                }
            }
            mLock.notifyAll();
        }
        if (session != null) patchCloseSession(session);
    }
}
