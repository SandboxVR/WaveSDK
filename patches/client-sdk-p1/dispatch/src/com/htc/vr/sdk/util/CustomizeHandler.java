package com.htc.vr.sdk.util;

import android.opengl.EGL14;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.TimeoutException;

/** Method donor only. The original constructors and EGL setup remain in the SDK. */
public class CustomizeHandler extends Handler {
    // Original fields referenced by the shutdown donor. Do not replace these fields.
    private EGLDisplay mEGLDisplay;
    private EGLContext mEGLContext;

    // Zero initialization makes this safe with every original constructor.
    private boolean mDispatchClosed;

    // Compile-only constructor. Never graft it into the original class.
    public CustomizeHandler(Looper looper) { super(looper); }

    private boolean isInSameThread() {
        return getLooper().getThread() == Thread.currentThread();
    }

    private boolean isMainThreadLooper() {
        return getLooper().getThread() == Looper.getMainLooper().getThread();
    }

    private void checkDispatchOpen() {
        if (mDispatchClosed) {
            throw new RejectedExecutionException("CustomizeHandler is closed");
        }
    }

    public void runOnThreadAsync(Runnable runnable) {
        boolean inline;
        synchronized (this) {
            checkDispatchOpen();
            inline = isInSameThread();
            if (!inline && !post(runnable)) {
                throw new RejectedExecutionException("Handler rejected asynchronous work");
            }
        }
        if (inline) runnable.run();
    }

    public void runOnUiThreadImmediateAsync(Runnable runnable) {
        boolean inline;
        synchronized (this) {
            checkDispatchOpen();
            inline = isInSameThread();
            if (!inline && !postAtFrontOfQueue(runnable)) {
                throw new RejectedExecutionException("Handler rejected immediate work");
            }
        }
        if (inline) runnable.run();
    }

    public void runOnThreadAsync(Runnable runnable, long delayMillis) {
        synchronized (this) {
            checkDispatchOpen();
            if (!postDelayed(runnable, delayMillis)) {
                throw new RejectedExecutionException("Handler rejected delayed work");
            }
        }
    }

    public void runOnThreadSync(Runnable runnable) {
        dispatchSync(runnable, 5000000000L);
    }

    public void runOnThreadSync(Runnable runnable, long timeout, int nanos) {
        if (timeout < 0 || nanos < 0 || nanos > 999999) {
            throw new IllegalArgumentException("Invalid timeout");
        }
        // Clamp before multiplication, including Long.MAX_VALUE. (0,0) is finite.
        long budget = timeout >= 5000 || (timeout == 0 && nanos == 0)
                ? 5000000000L : timeout * 1000000L + nanos;
        dispatchSync(runnable, budget);
    }

    private void dispatchSync(Runnable runnable, long budgetNanos) {
        SyncRunnable work = null;
        synchronized (this) {
            checkDispatchOpen();
            if (!isInSameThread()) {
                work = new SyncRunnable(runnable);
                if (!post(work)) {
                    throw new RejectedExecutionException("Handler rejected synchronous work");
                }
            }
        }
        if (work == null) runnable.run();
        else awaitCompletion(work, budgetNanos, true);
    }

    private void awaitCompletion(SyncRunnable work, long budgetNanos, boolean cancelQueued) {
        long start = System.nanoTime();
        IllegalStateException waitFailure = null;
        boolean interrupted = false;
        synchronized (work) {
            while (!work.mCompleted) {
                long remaining = budgetNanos - (System.nanoTime() - start);
                if (remaining <= 0) {
                    waitFailure = new IllegalStateException("Synchronous Handler work timed out",
                            new TimeoutException("Handler completion deadline expired"));
                    break;
                }
                try {
                    work.wait(remaining / 1000000L, (int) (remaining % 1000000L));
                } catch (InterruptedException failure) {
                    interrupted = true;
                    waitFailure = new IllegalStateException("Interrupted waiting for Handler work", failure);
                    break;
                }
            }
            if (waitFailure != null && cancelQueued && !work.mStarted) {
                work.mCancelled = true;
                work.mCompleted = true;
                work.notifyAll();
            }
            if (waitFailure != null) work.mWaiterDeparted = true;
        }
        // Never acquire the queue lock while holding the completion monitor.
        if (waitFailure != null) {
            if (cancelQueued) removeCallbacks(work);
            if (interrupted) Thread.currentThread().interrupt();
            throw waitFailure;
        }
        Throwable failure = work.mFailure;
        if (failure instanceof RuntimeException) throw (RuntimeException) failure;
        if (failure instanceof Error) throw (Error) failure;
        if (failure != null) throw new IllegalStateException("Handler work failed", failure);
    }

    public void quit() {
        SyncRunnable shutdown;
        synchronized (this) {
            if (mDispatchClosed) return;
            mDispatchClosed = true;
            // The main looper is shared and Android does not permit quitting it.
            if (isMainThreadLooper()) return;
            shutdown = new SyncRunnable(new ShutdownRunnable());
            // Always enqueue: inline destruction could overtake accepted queue work.
            if (!post(shutdown)) {
                throw new RejectedExecutionException("Handler rejected shutdown cleanup");
            }
        }
        // A target-thread caller must return to its queue to execute shutdown.
        if (!isInSameThread()) awaitCompletion(shutdown, 5000000000L, false);
    }

    private void shutdownOnThread() {
        try {
            // Same operations/ownership checks as the original CustomizeHandler$1.
            if (EGL14.eglGetCurrentContext() != EGL14.EGL_NO_CONTEXT) {
                if (mEGLDisplay != EGL14.EGL_NO_DISPLAY) {
                    if (!EGL14.eglMakeCurrent(mEGLDisplay, EGL14.EGL_NO_SURFACE,
                            EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT)) {
                        Log.e("CustomizeHandler", "eglMakeCurrent failed");
                    }
                    EGL14.eglDestroyContext(mEGLDisplay, mEGLContext);
                }
                mEGLDisplay = EGL14.EGL_NO_DISPLAY;
                mEGLContext = EGL14.EGL_NO_CONTEXT;
            }
        } finally {
            getLooper().quitSafely();
        }
    }

    final class ShutdownRunnable implements Runnable {
        public void run() {
            try {
                shutdownOnThread();
            } catch (RuntimeException failure) {
                Log.e("CustomizeHandler", "Shutdown cleanup failed", failure);
                throw failure;
            } catch (Error failure) {
                Log.e("CustomizeHandler", "Shutdown cleanup failed", failure);
                throw failure;
            }
        }
    }

    // Keeps the original binary name, outer reference, and constructor descriptor.
    class SyncRunnable implements Runnable {
        private Runnable mRunnable;
        boolean mStarted;
        boolean mCompleted;
        boolean mCancelled;
        boolean mWaiterDeparted;
        Throwable mFailure;

        public SyncRunnable(Runnable runnable) { mRunnable = runnable; }

        public void run() {
            synchronized (this) {
                if (mCancelled || mCompleted) return;
                mStarted = true;
            }
            Throwable failure = null;
            try {
                if (mRunnable != null) mRunnable.run();
            } catch (Throwable caught) {
                failure = caught;
            } finally {
                boolean reportFailure;
                synchronized (this) {
                    mFailure = failure;
                    mRunnable = null;
                    mCompleted = true;
                    reportFailure = mWaiterDeparted && failure != null;
                    notifyAll();
                }
                if (reportFailure) Log.e("CustomizeHandler", "Work failed after waiter returned", failure);
            }
        }
    }
}
