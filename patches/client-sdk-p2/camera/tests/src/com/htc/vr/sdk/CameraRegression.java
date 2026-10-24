package com.htc.vr.sdk;

import android.content.Context;
import android.media.ImageReader;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.RemoteException;
import android.view.Surface;
import com.htc.vr.core.DeprecatedCameraInfo;
import com.htc.vr.core.IVRCamera;
import com.htc.vr.core.IVRCameraListener;
import com.htc.vr.core.IVRCameraManager;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** Explicit host resource/queue models; no claim of Android ImageReader/JNI execution. */
public final class CameraRegression {
    static int checks;
    static final ConcurrentHashMap<Thread, Throwable> failures = new ConcurrentHashMap<Thread, Throwable>();
    static synchronized void check(boolean value, String message) {
        checks++;
        if (!value) throw new AssertionError(message);
    }
    static void await(CountDownLatch latch) {
        try { if (!latch.await(1500, TimeUnit.MILLISECONDS)) throw new AssertionError("latch timeout"); }
        catch (InterruptedException failure) { throw new AssertionError(failure); }
    }
    static Thread run(Runnable action) {
        Thread thread = new Thread(action); thread.setDaemon(true);
        thread.setUncaughtExceptionHandler((worker, failure) -> failures.put(worker, failure));
        thread.start(); return thread;
    }
    static void joined(Thread thread) throws Exception {
        thread.join(2500);
        check(!thread.isAlive(), "operation must return without deadlock");
        check(!failures.containsKey(thread), "worker assertion: " + failures.get(thread));
    }
    static void set(Object owner, String fieldName, Object value) throws Exception {
        Field field = RemoteVRCameraManager.class.getDeclaredField(fieldName);
        field.setAccessible(true); field.set(owner, value);
    }
    static Object get(Object owner, String fieldName) throws Exception {
        Field field = RemoteVRCameraManager.class.getDeclaredField(fieldName);
        field.setAccessible(true); return field.get(owner);
    }
    static final class Info extends DeprecatedCameraInfo {
        final int width, height;
        Info(int width, int height) { this.width = width; this.height = height; }
        public int getImgWidth() { return width; }
        public int getImgHeight() { return height; }
    }
    static final class Manager implements IVRCameraManager {
        int unregisters;
        boolean fail;
        public void unregisterCameraListener(IVRCameraListener listener) throws RemoteException {
            unregisters++; if (fail) throw new RemoteException();
        }
    }
    static final class Camera implements IVRCamera {
        final IBinder binder;
        volatile int registers, unregisters;
        boolean failRegister, failUnregister;
        Runnable inRegister, inUnregister;
        boolean failQuery;
        Camera() { this(new Binder()); }
        Camera(IBinder binder) { this.binder = binder; }
        public IBinder asBinder() { return binder; }
        public Bundle getVRCameraInfo(IBinder token) throws RemoteException {
            if (failQuery) throw new RemoteException(); return new Bundle();
        }
        public float[] getStereoCameraIntrinsic(IBinder token) throws RemoteException {
            if (failQuery) throw new RemoteException(); return new float[] {1, 2, 3, 4};
        }
        public void registerImageUpdateSurface(IBinder token, Surface surface) throws RemoteException {
            registers++; if (inRegister != null) inRegister.run();
            if (failRegister) throw new RemoteException();
        }
        public void unregisterImageUpdateSurface(IBinder token) throws RemoteException {
            unregisters++; if (inUnregister != null) inUnregister.run();
            if (failUnregister) throw new RemoteException();
        }
    }
    static final class Fixture {
        final Context context = new Context();
        final Manager service = new Manager();
        final Camera camera = new Camera();
        final RemoteVRCameraManager owner = new RemoteVRCameraManager(context, service);
        final AtomicInteger frames = new AtomicInteger();
        Fixture() throws Exception {
            set(owner, "mContext", context);
            set(owner, "mBinder", new Binder());
            set(owner, "mLock", new Object());
            set(owner, "mIVRCameraManager", service);
            set(owner, "mIVRCamera", camera);
            set(owner, "mDeprecatedCameraInfo", new Info(640, 480));
            set(owner, "mOnImageAvailableListener", (ImageReader.OnImageAvailableListener) reader -> {
                if (reader.hasImage.getAndSet(false)) frames.incrementAndGet();
            });
            owner.mListener = new RemoteVRCameraManager$2(owner);
        }
        ImageReader reader() throws Exception { return (ImageReader) get(owner, "mImageReader"); }
        void flush() throws Exception {
            CountDownLatch done = new CountDownLatch(1);
            check(reader().handler.post(() -> done.countDown()), "queue barrier accepted"); await(done);
        }
        void clear() throws Exception {
            check(!owner.mCameraStarted, "camera flag clear");
            check(get(owner, "mImageReader") == null, "reader owner detached");
            check(get(owner, "mBackgroundHandler") == null, "handler owner detached");
            check(get(owner, "mBackgroundThread") == null, "thread owner detached");
            check(get(owner, "mP2CameraSession") == null, "session reservation released");
        }
    }
    static void resetFailures() {
        ImageReader.failNew = false; ImageReader.failListener = false;
        ImageReader.failSurface = false; ImageReader.failDetach = false; ImageReader.failClose = false;
        ImageReader.beforeNew = null; Handler.failConstructor = false;
        Handler.rejectPosts = false;
        Handler.beforePost = null;
    }
    static void allocationFailures() throws Exception {
        for (int stage = 0; stage < 8; stage++) {
            resetFailures(); Fixture f = new Fixture();
            int readers = ImageReader.created.size(), threads = HandlerThread.created.size();
            if (stage == 0) set(f.owner, "mDeprecatedCameraInfo", null);
            if (stage == 1) set(f.owner, "mDeprecatedCameraInfo", new Info(640, 0));
            if (stage == 2) Handler.failConstructor = true;
            if (stage == 3) ImageReader.failNew = true;
            if (stage == 4) ImageReader.failListener = true;
            if (stage == 5) ImageReader.failSurface = true;
            if (stage == 6) f.camera.failRegister = true;
            if (stage == 7) Handler.rejectPosts = true;
            check(!f.owner.startCameraFromNative(), "native start fails at stage " + stage);
            check(!f.owner.mUserStarted, "failed native start never owns user flag");
            f.clear();
            for (int i = readers; i < ImageReader.created.size(); i++)
                check(ImageReader.created.get(i).closes == 1, "partial reader closes once");
            for (int i = threads; i < HandlerThread.created.size(); i++) {
                HandlerThread thread = HandlerThread.created.get(i);
                joined(thread); check(thread.quitCalls == 1, "partial thread quits once");
            }
            check(f.camera.unregisters == (stage >= 5 ? 1 : 0), "attempted remote registration reconciled");
            resetFailures(); set(f.owner, "mDeprecatedCameraInfo", new Info(640, 480));
            f.camera.failRegister = false;
            check(f.owner.startCameraFromNative(), "retry succeeds after rollback");
            f.owner.stopCameraFromNative(); f.clear();
        }
    }
    static void normalAndExceptionalStop() throws Exception {
        Fixture f = new Fixture(); check(f.owner.startCameraFromNative(), "normal native start");
        f.flush();
        ImageReader reader = f.reader(); HandlerThread thread = (HandlerThread) get(f.owner, "mBackgroundThread");
        ImageReader.OnImageAvailableListener captured = reader.listener;
        check(f.owner.startCamera() == 7, "duplicate start code retained");
        check(f.camera.registers == 1, "duplicate allocates no registration");
        reader.hasImage.set(true); captured.onImageAvailable(reader); check(f.frames.get() == 1, "active original callback delivered");
        f.camera.failUnregister = true; ImageReader.failDetach = true; ImageReader.failClose = true;
        f.owner.stopCameraFromNative(); resetFailures(); f.clear(); joined(thread);
        check(!f.owner.mUserStarted, "native stop clears user flag despite cleanup exceptions");
        check(reader.closes == 1 && thread.quitCalls == 1, "all local cleanup attempted");
        captured.onImageAvailable(reader); check(f.frames.get() == 1, "late callback suppressed before original reader access");
        f.owner.stopCameraFromNative(); check(reader.closes == 1, "duplicate stop harmless");
        check(f.camera.unregisters == 1, "remote unregister claimed once");

        Fixture pause = new Fixture(); check(pause.owner.startCameraFromNative(), "pause fixture starts");
        pause.owner.stopCamera(); check(pause.owner.mUserStarted, "lifecycle stop preserves desired user state");
        check(pause.owner.startCamera() == 0, "lifecycle resume starts again");
        pause.owner.stopCameraFromNative(); pause.clear();
    }
    static void cancelStart(boolean duringAllocation) throws Exception {
        Fixture f = new Fixture(); CountDownLatch entered = new CountDownLatch(1), resume = new CountDownLatch(1);
        Runnable blocked = () -> { entered.countDown(); await(resume); };
        if (duringAllocation) ImageReader.beforeNew = blocked; else f.camera.inRegister = blocked;
        AtomicInteger result = new AtomicInteger(-1);
        Thread start = run(() -> result.set(f.owner.startCamera())); await(entered);
        f.owner.stopCameraFromNative();
        check(f.owner.startCamera() == 99, "cancelled pending start retains reservation");
        resume.countDown(); joined(start); resetFailures();
        check(result.get() == 99, "cancelled start never commits"); f.clear();
        check(f.camera.unregisters == (duringAllocation ? 0 : 1), "pending registration rolled back once");
        ImageReader last = ImageReader.created.get(ImageReader.created.size() - 1);
        check(last.closes == 1, "cancelled start closes allocation published late");
        f.camera.inRegister = null;
        check(f.owner.startCamera() == 0, "restart after cancelled operation completes"); f.owner.stopCamera(); f.clear();
    }
    static void callbackStopAndRelease(boolean reentrant) throws Exception {
        Fixture f = new Fixture(); CountDownLatch entered = new CountDownLatch(1), resume = new CountDownLatch(1), done = new CountDownLatch(1);
        check(f.owner.startCamera() == 0, "callback fixture starts"); f.flush(); ImageReader reader = f.reader();
        Handler handler = reader.handler; HandlerThread thread = (HandlerThread) get(f.owner, "mBackgroundThread");
        ImageReader.OnImageAvailableListener captured = reader.listener;
        set(f.owner, "mOnImageAvailableListener", (ImageReader.OnImageAvailableListener) value -> {
            check(Thread.currentThread() == thread, "frame stays on original image handler thread");
            if (reentrant) f.owner.stopCameraFromNative();
            entered.countDown(); await(resume);
            check(reader.closes == 0, "reader stays alive through active callback");
        });
        check(handler.post(() -> { captured.onImageAvailable(reader); done.countDown(); }), "frame queued");
        await(entered);
        Thread release = run(() -> f.owner.releaseCameraResource()); joined(release);
        check(reader.closes == 0, "foreign release defers close without waiting on callback");
        resume.countDown(); await(done); joined(thread);
        check(reader.closes == 1 && thread.quitCalls == 1, "last callback closes reader and quits its own thread");
        f.clear(); check(f.service.unregisters == 1, "listener unregisters once");
        captured.onImageAvailable(reader);
        check(f.owner.startCamera() == 99, "released owner is terminal");
    }
    static void binderCallbacksAndWaiting() throws Exception {
        Fixture f = new Fixture(); Camera stale = new Camera();
        f.owner.mListener.onCameraUnavailable(stale);
        check(f.owner.getIVRCamera() == f.camera, "stale unavailability ignored");
        check(f.owner.startCamera() == 0, "Binder fixture starts"); ImageReader reader = f.reader();
        f.owner.mListener.onCameraUnavailable(new Camera(f.camera.binder));
        check(reader.closes == 1, "matching Binder wrapper closes local session"); f.clear();
        f.owner.mListener.onCameraAvailable(f.camera);
        check(f.owner.startCamera() == 0, "available service permits explicit restart");
        f.service.fail = true; f.owner.releaseCameraResource(); f.owner.releaseCameraResource();
        f.owner.mListener.onCameraAvailable(f.camera);
        check(f.owner.getIVRCamera() == null, "late availability cannot revive released owner");
        check(f.service.unregisters == 1, "repeated resource release does not repeat unregister");

        Fixture waiting = new Fixture(); set(waiting.owner, "mIVRCamera", null);
        CountDownLatch waitingStarted = new CountDownLatch(1);
        Thread waiter = run(() -> { waitingStarted.countDown(); check(waiting.owner.getIVRCamera() == null, "release wakes camera waiter"); });
        await(waitingStarted); waiting.owner.releaseCameraResource(); joined(waiter);
        Fixture interrupted = new Fixture(); set(interrupted.owner, "mIVRCamera", null);
        Thread interruption = run(() -> {
            Thread.currentThread().interrupt();
            check(interrupted.owner.getIVRCamera() == null, "interrupted wait returns unavailable");
            check(Thread.currentThread().isInterrupted(), "wait restores interrupt status");
        }); joined(interruption);
        Fixture signalled = new Fixture(); set(signalled.owner, "mIVRCamera", null);
        Thread signaller = run(() -> signalled.owner.mListener.onCameraAvailable(signalled.camera));
        check(signalled.owner.getIVRCamera() == signalled.camera, "availability wakes predicate waiter"); joined(signaller);
    }
    static void localClosePrecedesRemoteCleanup() throws Exception {
        Fixture f = new Fixture(); check(f.owner.startCamera() == 0, "remote order fixture starts");
        ImageReader reader = f.reader(); CountDownLatch entered = new CountDownLatch(1), resume = new CountDownLatch(1);
        f.camera.inUnregister = () -> { entered.countDown(); await(resume); };
        Thread stop = run(() -> f.owner.stopCamera()); await(entered);
        check(reader.closes == 1, "local close precedes slow remote unregister");
        check(f.owner.startCamera() == 99, "new start cannot race old registration cleanup");
        resume.countDown(); joined(stop); f.clear();
    }
    static void earlyFrameReplay() throws Exception {
        Fixture f = new Fixture();
        f.camera.inRegister = () -> {
            try {
                ImageReader reader = f.reader(); reader.hasImage.set(true);
                reader.listener.onImageAvailable(reader);
                check(f.frames.get() == 0, "in-flight registration cannot invoke original frame callback");
            } catch (Exception failure) { throw new AssertionError(failure); }
        };
        check(f.owner.startCamera() == 0, "registration with early frame commits"); f.flush();
        check(f.frames.get() == 1, "post-commit drain delivers early frame without another notification");
        f.owner.stopCamera(); f.clear();
    }
    static void queryAbsenceAndDeadline() throws Exception {
        Fixture f = new Fixture(); Method info = RemoteVRCameraManager.class.getDeclaredMethod("getVRCameraInfo");
        info.setAccessible(true);
        check(info.invoke(f.owner) != null, "camera info cache returned");
        check(f.owner.getCameraIntrinsic().length == 4, "intrinsic query returned");
        f.camera.failQuery = true; set(f.owner, "mDeprecatedCameraInfo", null);
        check(info.invoke(f.owner) == null, "info remote exception returns unavailable");
        check(f.owner.getCameraIntrinsic() == null, "intrinsic remote exception returns unavailable");
        f.owner.releaseCameraResource();
        check(info.invoke(f.owner) == null, "released info query is null-safe");
        check(f.owner.getCameraIntrinsic() == null, "released intrinsic query is null-safe");
        Fixture timeout = new Fixture(); set(timeout.owner, "mIVRCamera", null);
        Object lock = get(timeout.owner, "mLock");
        Thread spurious = run(() -> { for (int i = 0; i < 100; i++) synchronized (lock) { lock.notifyAll(); } });
        long began = System.nanoTime();
        check(timeout.owner.getIVRCamera() == null, "missing service returns unavailable after deadline");
        long elapsedMs = (System.nanoTime() - began) / 1000000L;
        check(elapsedMs >= 1700 && elapsedMs < 3500, "predicate wait uses bounded monotonic deadline"); joined(spurious);
        Thread.currentThread().interrupt();
        set(timeout.owner, "mDeprecatedCameraInfo", null);
        check(info.invoke(timeout.owner) == null, "interrupted missing-service info query is null-safe");
        check(timeout.owner.getCameraIntrinsic() == null, "interrupted missing-service intrinsic is null-safe");
        check(Thread.interrupted(), "query preserves interrupt status");
    }
    static void replacementAndReleaseWhileStarting() throws Exception {
        Fixture f = new Fixture(); check(f.owner.startCamera() == 0, "replacement fixture starts");
        ImageReader old = f.reader(); Camera replacement = new Camera();
        CountDownLatch retired = new CountDownLatch(1);
        f.camera.inUnregister = () -> retired.countDown();
        f.owner.mListener.onCameraAvailable(replacement);
        await(retired);
        long deadline = System.nanoTime() + 1500000000L;
        while (get(f.owner, "mP2CameraSession") != null && System.nanoTime() < deadline) Thread.yield();
        check(old.closes == 1 && f.camera.unregisters == 1, "replacement closes old service session"); f.clear();
        f.owner.mListener.onCameraUnavailable(f.camera);
        check(f.owner.getIVRCamera() == replacement, "old service loss cannot clear replacement");
        check(f.owner.startCamera() == 0, "replacement service can start"); f.owner.stopCamera();

        Fixture released = new Fixture(); CountDownLatch entered = new CountDownLatch(1), resume = new CountDownLatch(1);
        released.camera.inRegister = () -> { entered.countDown(); await(resume); };
        AtomicInteger result = new AtomicInteger(-1);
        Thread start = run(() -> result.set(released.owner.startCamera())); await(entered);
        ImageReader reader = released.reader(); released.owner.releaseCameraResource();
        check(reader.closes == 1, "release closes reader before pending remote registration returns");
        released.owner.mListener.onCameraAvailable(released.camera);
        resume.countDown(); joined(start); check(result.get() == 99, "release prevents pending commit");
        released.clear(); check(released.camera.unregisters == 1, "late registration gets one rollback unregister");
        check(released.owner.getIVRCamera() == null, "late callback cannot revive owner during pending release");
    }
    static void replacementRetirementIsBounded() throws Exception {
        Fixture f = new Fixture(); check(f.owner.startCamera() == 0, "retirement fixture starts");
        ImageReader reader = f.reader(); CountDownLatch entered = new CountDownLatch(1), resume = new CountDownLatch(1), finished = new CountDownLatch(1);
        Thread[] cleanup = new Thread[1];
        f.camera.inUnregister = () -> { cleanup[0] = Thread.currentThread(); entered.countDown(); await(resume); finished.countDown(); };
        Camera replacement = new Camera();
        Thread availability = run(() -> f.owner.mListener.onCameraAvailable(replacement));
        joined(availability); await(entered);
        check(reader.closes == 1, "availability closes local resources before blocked retirement");
        check(cleanup[0] != availability && cleanup[0].isDaemon(), "retirement IPC runs on dedicated daemon outside Binder callback");
        check("RemoteVRCameraRetirement".equals(cleanup[0].getName()), "retirement worker has bounded identity");
        check(f.owner.startCamera() == 99, "retirement reserves session while old unregister blocks");
        for (int i = 0; i < 5; i++) f.owner.mListener.onCameraAvailable(new Camera());
        check(f.camera.unregisters == 1, "replacement storm creates only one unregister worker");
        Camera latest = (Camera) f.owner.getIVRCamera();
        f.owner.mListener.onCameraUnavailable(latest);
        f.owner.mListener.onCameraAvailable(replacement);
        check(f.owner.startCamera() == 99, "service loss cannot drop reservation owned by old unregister");
        resume.countDown(); await(finished); joined(cleanup[0]); f.clear();
        check(f.owner.startCamera() == 0, "new service starts after retirement finishes"); f.owner.stopCamera(); f.clear();
    }
    static void callbackAtDrainAcceptance(boolean rejected) throws Exception {
        Fixture f = new Fixture(); CountDownLatch ready = new CountDownLatch(1);
        Thread[] notification = new Thread[1];
        Handler.beforePost = () -> {
            try {
                ImageReader reader = f.reader(); reader.hasImage.set(true);
                ImageReader.OnImageAvailableListener callback = reader.listener;
                notification[0] = run(() -> { ready.countDown(); callback.onImageAvailable(reader); });
                await(ready);
            } catch (Exception failure) { throw new AssertionError(failure); }
        };
        Handler.rejectPosts = rejected;
        check(f.owner.startCamera() == (rejected ? 99 : 0), "drain acceptance determines start result");
        Handler.beforePost = null; Handler.rejectPosts = false; joined(notification[0]);
        if (!rejected) f.flush();
        check(f.frames.get() == (rejected ? 0 : 1), "queued frame cannot precede successful session commit");
        f.owner.stopCamera(); f.clear();
    }
    static void duplicateBinderDuringStart() throws Exception {
        Fixture f = new Fixture();
        f.camera.inRegister = () -> f.owner.mListener.onCameraAvailable(new Camera(f.camera.binder));
        check(f.owner.startCamera() == 0, "duplicate Binder wrapper cannot cancel a pending start");
        check(f.owner.getIVRCamera() == f.camera, "duplicate availability preserves current service snapshot");
        f.owner.stopCamera(); f.clear();
    }
    public static void main(String[] args) throws Exception {
        allocationFailures(); normalAndExceptionalStop(); cancelStart(true); cancelStart(false);
        callbackStopAndRelease(false); callbackStopAndRelease(true); binderCallbacksAndWaiting();
        localClosePrecedesRemoteCleanup(); earlyFrameReplay(); queryAbsenceAndDeadline();
        replacementAndReleaseWhileStarting();
        callbackAtDrainAcceptance(false); callbackAtDrainAcceptance(true);
        replacementRetirementIsBounded();
        duplicateBinderDuringStart();
        for (HandlerThread thread : HandlerThread.created) { joined(thread); check(thread.uncaught == null, "no callback worker error"); }
        System.out.println("Camera regressions passed: " + checks + " assertions.");
    }
}
