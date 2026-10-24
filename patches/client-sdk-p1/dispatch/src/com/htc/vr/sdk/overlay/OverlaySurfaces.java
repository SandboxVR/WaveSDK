package com.htc.vr.sdk.overlay;

import android.util.Log;
import com.htc.vr.sdk.util.CustomizeHandler;
import java.util.List;
import java.util.concurrent.RejectedExecutionException;

/** Method donor only; every original constructor, surface implementation and reader stays. */
public class OverlaySurfaces {
    private CustomizeHandler mEglHandler;
    private List<OverlaySurface> mOverlaySurfaces;
    private boolean mEnableFrameAvailable;
    // Added field, safely zero-initialized by every original constructor.
    private volatile boolean mClosing;

    // Graft over the existing synthetic field getter used by frame callbacks.
    static boolean patchFrameAvailable(OverlaySurfaces owner) {
        return !owner.mClosing && owner.mEnableFrameAvailable;
    }

    public synchronized void add(int id, boolean protectedContent, int count) {
        if (mClosing || mEglHandler == null) {
            throw new RejectedExecutionException("OverlaySurfaces is closed");
        }
        mEglHandler.runOnThreadSync(
                new OverlaySurfaces$$ExternalSyntheticLambda0(this, count, id, protectedContent));
    }

    public synchronized void remove(int id) {
        if (mClosing || mEglHandler == null) {
            throw new RejectedExecutionException("OverlaySurfaces is closed");
        }
        mEglHandler.runOnThreadSync(new OverlaySurfaces$1(this, id));
    }

    public synchronized void quit() {
        if (mClosing) return;
        mClosing = true;
        CustomizeHandler handler = mEglHandler;
        if (handler == null) return;
        // Keep the handler visible to any accepted add still constructing listeners.
        if (!handler.post(new OverlaySurfaces$$ExternalSyntheticLambda1(this))) {
            mEglHandler = null;
            throw new RejectedExecutionException("OverlaySurfaces shutdown was rejected");
        }
    }

    // Graft this body under the original lambda$quit$0$com-htc-vr-sdk-overlay-OverlaySurfaces name.
    void patchQuitOnThread() {
        CustomizeHandler handler = mEglHandler;
        Throwable failure = null;
        try {
            for (OverlaySurface surface : mOverlaySurfaces) {
                try {
                    surface.release();
                } catch (Throwable caught) {
                    if (failure == null) failure = caught;
                    else if (failure != caught) failure.addSuppressed(caught);
                }
            }
        } catch (Throwable caught) {
            if (failure == null) failure = caught;
            else if (failure != caught) failure.addSuppressed(caught);
        } finally {
            try {
                mOverlaySurfaces.clear();
            } catch (Throwable caught) {
                if (failure == null) failure = caught;
                else if (failure != caught) failure.addSuppressed(caught);
            } finally {
                try {
                    if (handler != null) handler.quit();
                } catch (Throwable caught) {
                    if (failure == null) failure = caught;
                    else if (failure != caught) failure.addSuppressed(caught);
                } finally {
                    synchronized (this) { mEglHandler = null; }
                }
            }
        }
        // Preserve asynchronous quit semantics; report failure without killing the
        // looper before its queued EGL cleanup has a chance to run.
        if (failure != null) Log.e("OverlaySurfaces", "Surface shutdown failed", failure);
    }

    // Compile-only type stub. Never copy it into the packaged SDK.
    static class OverlaySurface { public void release() { } }
}
