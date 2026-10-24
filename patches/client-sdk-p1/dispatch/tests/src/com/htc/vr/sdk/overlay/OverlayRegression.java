package com.htc.vr.sdk.overlay;

import android.os.Looper;
import android.util.Log;
import com.htc.vr.sdk.util.CustomizeHandler;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Donor lifecycle bodies with stand-ins for unchanged GL add/remove callbacks. */
public final class OverlayRegression {
    static volatile Runnable addCallback = () -> { };
    static final AtomicInteger adds = new AtomicInteger(), removes = new AtomicInteger();
    private static int checks;
    private static void check(boolean result, String message) {
        if (!result) throw new AssertionError(message);
        checks++;
    }
    private static Object field(OverlaySurfaces owner, String name) throws Exception {
        Field field = OverlaySurfaces.class.getDeclaredField(name); field.setAccessible(true);
        return field.get(owner);
    }
    private static void set(OverlaySurfaces owner, String name, Object value) throws Exception {
        Field field = OverlaySurfaces.class.getDeclaredField(name); field.setAccessible(true); field.set(owner, value);
    }
    private static void rejected(Runnable action) {
        try { action.run(); } catch (RejectedExecutionException expected) { checks++; return; }
        throw new AssertionError("closed overlay operation must reject");
    }
    private static void await(CountDownLatch latch) {
        try { if (!latch.await(2, TimeUnit.SECONDS)) throw new AssertionError("latch deadline"); }
        catch (InterruptedException failure) { throw new AssertionError(failure); }
    }
    private static OverlaySurfaces owner(CustomizeHandler handler, List<OverlaySurfaces.OverlaySurface> surfaces) throws Exception {
        OverlaySurfaces owner = new OverlaySurfaces();
        set(owner, "mEglHandler", handler); set(owner, "mOverlaySurfaces", surfaces); return owner;
    }
    private static void releaseContinuesAndCloses() throws Exception {
        Looper queue = new Looper(); CustomizeHandler handler = new CustomizeHandler(queue);
        AtomicInteger releases = new AtomicInteger(); RuntimeException failure = new IllegalStateException("release failure");
        List<OverlaySurfaces.OverlaySurface> surfaces = new ArrayList<OverlaySurfaces.OverlaySurface>();
        surfaces.add(new OverlaySurfaces.OverlaySurface() {
            public void release() { releases.incrementAndGet(); throw failure; }
        });
        surfaces.add(new OverlaySurfaces.OverlaySurface() {
            public void release() { releases.incrementAndGet(); }
        });
        OverlaySurfaces owner = owner(handler, surfaces);
        set(owner, "mEnableFrameAvailable", true);
        check(OverlaySurfaces.patchFrameAvailable(owner), "frame callbacks enabled before closure");
        owner.quit(); owner.quit();
        check(!OverlaySurfaces.patchFrameAvailable(owner), "queued frame callback rejected as soon as shutdown is requested");
        set(owner, "mEnableFrameAvailable", true);
        check(!OverlaySurfaces.patchFrameAvailable(owner), "frame enable setter cannot reopen closed lifecycle");
        rejected(() -> owner.add(1, false, 1)); rejected(() -> owner.remove(1));
        queue.awaitExit();
        check(releases.get() == 2, "a release failure must not skip remaining surfaces");
        check(surfaces.isEmpty(), "surface references cleared despite release failure");
        check(field(owner, "mEglHandler") == null, "handler cleared after cleanup");
        check(queue.quitCalls == 1, "failed surface release still shuts down handler exactly once");
        check(Log.failures.contains(failure), "asynchronous release error is observable");
        check(queue.uncaught == null, "release error cannot kill looper ahead of EGL cleanup");
    }
    private static void rejectedShutdownIsIdempotent() throws Exception {
        Looper queue = new Looper(); CustomizeHandler handler = new CustomizeHandler(queue);
        OverlaySurfaces owner = owner(handler, new ArrayList<OverlaySurfaces.OverlaySurface>());
        queue.quitSafely(); queue.awaitExit();
        rejected(owner::quit); owner.quit();
        rejected(() -> owner.add(1, false, 1)); rejected(() -> owner.remove(1));
        check(field(owner, "mEglHandler") == null, "rejected cleanup cannot leave a usable handler reference");
    }
    private static void admissionVersusQuit() throws Exception {
        Looper queue = new Looper(); CustomizeHandler handler = new CustomizeHandler(queue);
        OverlaySurfaces owner = owner(handler, new ArrayList<OverlaySurfaces.OverlaySurface>());
        CountDownLatch started = new CountDownLatch(1), release = new CountDownLatch(1);
        addCallback = () -> { started.countDown(); await(release); };
        AtomicReference<Throwable> failed = new AtomicReference<Throwable>();
        Thread add = new Thread(() -> {
            try { owner.add(4, false, 1); } catch (Throwable failure) { failed.set(failure); }
        });
        Thread quit = new Thread(() -> {
            try { owner.quit(); } catch (Throwable failure) { failed.set(failure); }
        });
        add.start(); await(started); quit.start();
        check(field(owner, "mEglHandler") == handler, "accepted add retains listener handler until it finishes");
        release.countDown(); add.join(1500); quit.join(1500);
        check(!add.isAlive() && !quit.isAlive() && failed.get() == null, "add/quit serialize without deadlock");
        queue.awaitExit();
        rejected(() -> owner.add(5, false, 1)); rejected(() -> owner.remove(4));
        check(adds.get() == 1 && removes.get() == 0, "no producer runs after closure");
        addCallback = () -> { };
    }
    private static void queuedFrameAfterRelease() throws Exception {
        Looper queue = new Looper(); CustomizeHandler handler = new CustomizeHandler(queue);
        List<OverlaySurfaces.OverlaySurface> surfaces = new ArrayList<OverlaySurfaces.OverlaySurface>();
        AtomicInteger releases = new AtomicInteger(), resourceReads = new AtomicInteger();
        surfaces.add(new OverlaySurfaces.OverlaySurface() { public void release() { releases.incrementAndGet(); } });
        OverlaySurfaces owner = owner(handler, surfaces);
        set(owner, "mEnableFrameAvailable", true);
        handler.runOnThreadSync(() -> {
            if (OverlaySurfaces.patchFrameAvailable(owner)) resourceReads.incrementAndGet();
        });
        CountDownLatch blocked = new CountDownLatch(1), releaseQueue = new CountDownLatch(1);
        handler.post(() -> { blocked.countDown(); await(releaseQueue); }); await(blocked);
        owner.quit();
        AtomicInteger callbackAfterRelease = new AtomicInteger();
        check(handler.post(() -> {
            if (releases.get() == 1) callbackAfterRelease.incrementAndGet();
            if (OverlaySurfaces.patchFrameAvailable(owner)) resourceReads.incrementAndGet();
        }), "framework callback can already be queued behind shutdown");
        releaseQueue.countDown(); queue.awaitExit();
        check(callbackAfterRelease.get() == 1, "test callback executed after surface release");
        check(resourceReads.get() == 1, "queued callback cannot access resources after shutdown");
    }
    public static void main(String[] args) throws Exception {
        Looper.getMainLooper();
        releaseContinuesAndCloses(); rejectedShutdownIsIdempotent(); admissionVersusQuit(); queuedFrameAfterRelease();
        System.out.println("PASS overlay lifecycle donor: " + checks + " assertions");
    }
}
