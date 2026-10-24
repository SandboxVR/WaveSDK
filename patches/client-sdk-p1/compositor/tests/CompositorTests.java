package com.htc.vr.core;

import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import androidx.lifecycle.Lifecycle;
import com.htc.vr.core.lifecycle.ManualLifecycleOwner;
import com.htc.vr.sdk.DoubleWideRenderer;
import com.htc.vr.sdk.VRActivityDelegate;
import com.htc.vr.sdk.VRAndroidContext;
import java.lang.ref.WeakReference;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Host tests execute the real helpers and the two Java donor callback classes. */
public final class CompositorTests {
    private static int checks;
    private static final AtomicReference<Throwable> backgroundFailure = new AtomicReference<Throwable>();

    private static final class Token implements IBinder {
        final String name;
        Token(String name) { this.name = name; }
        @Override public String toString() { return name; }
    }
    private static final class Proxy implements IVRCompositor {
        final IBinder token;
        Proxy(String name) { token = new Token(name); }
        Proxy(IBinder token) { this.token = token; }
        @Override public IBinder asBinder() { return token; }
    }
    private static class Recording extends VRCompositorManager.Listener {
        final List<String> events = new ArrayList<String>();
        @Override public void onCompositorAvailable(VRCompositor compositor) {
            check(Looper.myLooper() == Looper.getMainLooper(), "available on main");
            check(current(this) == compositor, "available field published before callback");
            events.add("+" + compositor.binder.asBinder());
        }
        @Override public void onCompositorUnavailable(VRCompositor compositor) {
            check(Looper.myLooper() == Looper.getMainLooper(), "unavailable on main");
            check(current(this) == null, "unavailable clears field before callback");
            events.add("-" + compositor.binder.asBinder());
        }
    }
    private static final class Delegate extends VRActivityDelegate {
        int kills;
        VRCompositorManager.Listener listener;
        @Override public void killOnUiThread() {
            check(Looper.myLooper() == Looper.getMainLooper(), "SDK kill on main");
            check(ClientSdkP1Compositor.isTerminal(listener), "terminal precedes process exit");
            check(current(listener) == null, "listener cleared before process exit");
            kills++;
        }
    }

    public static void main(String[] args) throws Exception {
        normalLifecycle();
        pendingEvents();
        handlerRejection();
        queuedTimeout();
        interruption();
        callbackFailures();
        reentrantLoss();
        runningTimeout();
        initializationLossTimeout();
        sdkPolicy();
        weakOwner();
        check(backgroundFailure.get() == null, "no uncaught worker failure: " + backgroundFailure.get());
        System.out.println("PASS CompositorTests: " + checks + " assertions");
    }

    private static void normalLifecycle() {
        Recording listener = new Recording();
        Proxy first = new Proxy("A"), second = new Proxy("B"), third = new Proxy("C");
        available(listener, first);
        check(listener.events.isEmpty(), "availability remains asynchronous");
        handler(listener).pump();
        VRCompositor original = current(listener);
        available(listener, new Proxy(first.token));
        handler(listener).pump();
        check(current(listener) == original, "same underlying Binder proxy is idempotent");
        lossOnMain(listener, first);
        check(current(listener) == null, "same-main loss completes inline");
        lossOnMain(listener, first);
        available(listener, second);
        handler(listener).pump();
        lossOnMain(listener, first);
        check(current(listener).binder == second, "stale old Binder loss is ignored");
        available(listener, third);
        handler(listener).pump();
        equal(listener.events, "+A", "-A", "+B", "-B", "+C");
    }

    private static void pendingEvents() {
        Recording listener = new Recording();
        Proxy first = new Proxy("A"), second = new Proxy("B");
        available(listener, first);
        available(listener, second);
        handler(listener).pump();
        equal(listener.events, "+B");
        lossOnMain(listener, second);
        available(listener, first);
        lossOnMain(listener, first);
        handler(listener).pump();
        equal(listener.events, "+B", "-B");
        check(current(listener) == null, "undelivered available/loss pair owns no compositor");
    }

    private static void handlerRejection() {
        Recording listener = new Recording();
        Proxy first = new Proxy("A");
        available(listener, first);
        handler(listener).pump();
        handler(listener).reject = true;
        long start = System.nanoTime();
        listener.getBinder$common_core_release().compositorUnavailable(first);
        check(elapsedMillis(start) < 500, "rejected disconnect post returns promptly");
        check(current(listener) != null, "rejected cleanup cannot mutate main state");
        available(listener, new Proxy("B"));
        handler(listener).pump();
        equal(listener.events, "+A");
        handler(listener).reject = false;
        handler(listener).throwOnPost = true;
        start = System.nanoTime();
        listener.getBinder$common_core_release().compositorUnavailable(first);
        check(elapsedMillis(start) < 500, "throwing Handler.post returns promptly");
        handler(listener).throwOnPost = false;
        available(listener, new Proxy("C"));
        handler(listener).pump();
        equal(listener.events, "+A", "-A", "+C");
    }

    private static void queuedTimeout() throws Exception {
        Recording listener = new Recording();
        Proxy first = new Proxy("A");
        available(listener, first);
        handler(listener).pump();
        long start = System.nanoTime();
        Thread worker = disconnectThread(listener, first, null);
        worker.join(4000);
        check(!worker.isAlive(), "queued disconnect has bounded wait");
        check(elapsedMillis(start) >= 1500 && elapsedMillis(start) < 4000, "two-second timeout used");
        handler(listener).pump();
        equal(listener.events, "+A");
        check(current(listener) != null, "timed-out event performs no late state mutation");
        available(listener, new Proxy("B"));
        handler(listener).pump();
        equal(listener.events, "+A", "-A", "+B");

        Recording pending = new Recording();
        available(pending, first);
        worker = disconnectThread(pending, first, null);
        worker.join(4000);
        check(!worker.isAlive(), "pending connect cancellation has bounded loss wait");
        handler(pending).pump();
        check(pending.events.isEmpty(), "loss cancels undelivered stale connect even after timeout");
    }

    private static void interruption() throws Exception {
        final Recording listener = new Recording();
        final Proxy first = new Proxy("A");
        available(listener, first);
        handler(listener).pump();
        final boolean[] restored = {false};
        Thread worker = thread(new Runnable() {
            @Override public void run() {
                listener.getBinder$common_core_release().compositorUnavailable(first);
                restored[0] = Thread.currentThread().isInterrupted();
            }
        });
        worker.start();
        waiting(worker);
        worker.interrupt();
        worker.join(1000);
        check(!worker.isAlive() && restored[0], "interrupt status restored and caller returns");
        handler(listener).pump();
        equal(listener.events, "+A");
    }

    private static void callbackFailures() {
        Recording failingAvailable = new Recording() {
            @Override public void onCompositorAvailable(VRCompositor compositor) {
                throw new IllegalArgumentException("availability failure");
            }
        };
        available(failingAvailable, new Proxy("A"));
        boolean threw = false;
        try { handler(failingAvailable).pump(); }
        catch (IllegalArgumentException expected) { threw = true; }
        check(threw, "availability keeps original fatal main-looper exception semantics");
        check(ClientSdkP1Compositor.isTerminal(failingAvailable), "availability failure closes listener");
        check(current(failingAvailable) == null, "availability failure clears owner field");
        available(failingAvailable, new Proxy("B"));
        handler(failingAvailable).pump();

        Recording failingLoss = new Recording() {
            @Override public void onCompositorUnavailable(VRCompositor compositor) {
                check(current(this) == null, "throwing loss sees cleared field");
                throw new IllegalArgumentException("loss failure");
            }
        };
        Proxy first = new Proxy("A");
        available(failingLoss, first);
        handler(failingLoss).pump();
        threw = false;
        try { lossOnMain(failingLoss, first); }
        catch (IllegalStateException expected) { threw = true; }
        check(threw && ClientSdkP1Compositor.isTerminal(failingLoss), "loss failure reported without reopening owner");
        available(failingLoss, new Proxy("B"));
        handler(failingLoss).pump();
        equal(failingLoss.events, "+A");
    }

    private static void reentrantLoss() {
        final Proxy first = new Proxy("A");
        Recording listener = new Recording() {
            @Override public void onCompositorAvailable(VRCompositor compositor) {
                super.onCompositorAvailable(compositor);
                getBinder$common_core_release().compositorUnavailable(first);
            }
        };
        available(listener, first);
        handler(listener).pump();
        equal(listener.events, "+A", "-A");
        check(current(listener) == null, "reentrant loss drains in order without waiting on itself");
    }

    private static void runningTimeout() throws Exception {
        final CountDownLatch entered = new CountDownLatch(1), release = new CountDownLatch(1);
        final Recording listener = new Recording() {
            @Override public void onCompositorUnavailable(VRCompositor compositor) {
                super.onCompositorUnavailable(compositor);
                entered.countDown();
                try {
                    if (!release.await(5, TimeUnit.SECONDS)) throw new AssertionError("release timeout");
                } catch (InterruptedException failure) { throw new AssertionError(failure); }
                ClientSdkP1Compositor.terminate(this);
            }
        };
        Proxy first = new Proxy("A");
        available(listener, first);
        handler(listener).pump();
        Thread worker = disconnectThread(listener, first, null);
        waiting(worker);
        Thread main = thread(new Runnable() { @Override public void run() { handler(listener).pump(); } });
        main.start();
        check(entered.await(1, TimeUnit.SECONDS), "disconnect callback started");
        worker.join(3500);
        check(!worker.isAlive(), "caller bounded even while callback is already executing");
        available(listener, new Proxy("B"));
        release.countDown();
        main.join(1000);
        check(!main.isAlive(), "already executing callback can finish safely");
        equal(listener.events, "+A", "-A");
        check(ClientSdkP1Compositor.isTerminal(listener), "terminal callback prevents replacement after timeout");
    }

    private static void sdkPolicy() throws Exception {
        DoubleWideRenderer.allocations = 0;
        Delegate delegate = delegate();
        Proxy first = new Proxy("A"), second = new Proxy("B");
        available(delegate.listener, first);
        handler(delegate.listener).pump();
        check(DoubleWideRenderer.allocations == 1, "SDK first connection creates renderer");
        available(delegate.listener, new Proxy(first.token));
        handler(delegate.listener).pump();
        check(DoubleWideRenderer.allocations == 1, "SDK duplicate creates no second renderer");
        lossOnMain(delegate.listener, new Proxy("stale"));
        check(delegate.kills == 0, "stale loss does not terminate SDK process");
        lossOnMain(delegate.listener, first);
        check(delegate.kills == 1, "matched SDK disconnect calls original process-exit path");
        available(delegate.listener, second);
        handler(delegate.listener).pump();
        lossOnMain(delegate.listener, first);
        check(delegate.kills == 1 && DoubleWideRenderer.allocations == 1, "returning kill stub cannot reopen or kill twice");

        Delegate replacement = delegate();
        available(replacement.listener, first);
        handler(replacement.listener).pump();
        available(replacement.listener, second);
        handler(replacement.listener).pump();
        check(replacement.kills == 1 && DoubleWideRenderer.allocations == 2, "active SDK replacement exits before allocating second renderer");

        Delegate destroyed = delegate();
        ManualLifecycleOwner owner = (ManualLifecycleOwner) field(VRActivityDelegate.class, "mLifecycleOwner").get(destroyed);
        owner.getLifecycle().state = Lifecycle.State.DESTROYED;
        available(destroyed.listener, first);
        handler(destroyed.listener).pump();
        check(destroyed.kills == 0 && DoubleWideRenderer.allocations == 2, "destroyed owner never creates renderer");
        check(current(destroyed.listener) == null && ClientSdkP1Compositor.isTerminal(destroyed.listener), "destroyed callback closes listener");
    }

    private static void initializationLossTimeout() throws Exception {
        final CountDownLatch entered = new CountDownLatch(1), release = new CountDownLatch(1);
        final Recording listener = new Recording() {
            @Override public void onCompositorAvailable(VRCompositor compositor) {
                super.onCompositorAvailable(compositor);
                entered.countDown();
                try {
                    if (!release.await(5, TimeUnit.SECONDS)) throw new AssertionError("release timeout");
                } catch (InterruptedException failure) { throw new AssertionError(failure); }
            }
        };
        Proxy first = new Proxy("A");
        available(listener, first);
        Thread main = thread(new Runnable() { @Override public void run() { handler(listener).pump(); } });
        main.start();
        check(entered.await(1, TimeUnit.SECONDS), "availability initialization started");
        Thread worker = disconnectThread(listener, first, null);
        worker.join(3500);
        check(!worker.isAlive(), "loss caller bounded during consumer initialization");
        release.countDown();
        main.join(1000);
        check(!main.isAlive(), "invalidated initialization completes");
        equal(listener.events, "+A", "-A");
        check(current(listener) == null, "canceled in-flight initialization cannot leave lost compositor published");
        available(listener, new Proxy("B"));
        handler(listener).pump();
        equal(listener.events, "+A", "-A", "+B");
    }

    private static Delegate delegate() throws Exception {
        Delegate delegate = new Delegate();
        field(VRActivityDelegate.class, "mVRContext").set(delegate, new VRAndroidContext());
        field(VRActivityDelegate.class, "mLifecycleOwner").set(delegate, new ManualLifecycleOwner());
        delegate.listener = (VRCompositorManager.Listener) field(VRActivityDelegate.class, "mCompositorListener").get(delegate);
        return delegate;
    }

    private static WeakReference<VRCompositorManager.Listener> unusedOwner() {
        Recording listener = new Recording();
        Proxy first = new Proxy("A");
        available(listener, first);
        handler(listener).pump();
        lossOnMain(listener, first);
        handler(listener).pump();
        return new WeakReference<VRCompositorManager.Listener>(listener);
    }

    private static WeakReference<VRCompositorManager.Listener> connectedOwner() {
        final Recording listener = new Recording();
        IBinder capturingToken = new IBinder() {
            @SuppressWarnings("unused") final Object callbackOwner = listener;
        };
        available(listener, new Proxy(capturingToken));
        handler(listener).pump();
        return new WeakReference<VRCompositorManager.Listener>(listener);
    }

    private static void weakOwner() throws Exception {
        WeakReference<VRCompositorManager.Listener> owner = unusedOwner();
        for (int attempt = 0; owner.get() != null && attempt < 100; attempt++) {
            System.gc();
            Thread.sleep(10);
        }
        check(owner.get() == null, "dispatcher registry does not retain completed listener");
        owner = connectedOwner();
        for (int attempt = 0; owner.get() != null && attempt < 100; attempt++) {
            System.gc();
            Thread.sleep(10);
        }
        check(owner.get() == null, "local Binder capturing listener does not create registry retention");
    }

    private static Thread disconnectThread(final VRCompositorManager.Listener listener, final Proxy proxy,
            final AtomicReference<Throwable> failure) {
        Thread worker = thread(new Runnable() {
            @Override public void run() {
                try { listener.getBinder$common_core_release().compositorUnavailable(proxy); }
                catch (Throwable problem) {
                    if (failure != null) failure.set(problem);
                    else throw new AssertionError(problem);
                }
            }
        });
        worker.start();
        return worker;
    }

    private static void waiting(Thread thread) {
        long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(1);
        while (thread.isAlive() && thread.getState() != Thread.State.TIMED_WAITING && System.nanoTime() < deadline) Thread.yield();
        check(thread.isAlive() && thread.getState() == Thread.State.TIMED_WAITING, "worker waits for main dispatch");
    }
    private static Thread thread(Runnable action) {
        Thread thread = new Thread(action);
        thread.setUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override public void uncaughtException(Thread thread, Throwable failure) {
                backgroundFailure.compareAndSet(null, failure);
            }
        });
        return thread;
    }
    private static Field field(Class<?> type, String name) throws Exception {
        Field field = type.getDeclaredField(name);
        field.setAccessible(true);
        return field;
    }
    private static void available(VRCompositorManager.Listener listener, Proxy proxy) {
        listener.getBinder$common_core_release().compositorAvailable(proxy, new Bundle());
    }
    private static void lossOnMain(final VRCompositorManager.Listener listener, final Proxy proxy) {
        Looper.asMain(new Runnable() { @Override public void run() { listener.getBinder$common_core_release().compositorUnavailable(proxy); } });
    }
    private static Handler handler(VRCompositorManager.Listener listener) { return VRCompositorManager.Listener.access$getMHandler$p(listener); }
    private static VRCompositor current(VRCompositorManager.Listener listener) { return VRCompositorManager.Listener.access$getMCompositor$p(listener); }
    private static long elapsedMillis(long start) { return TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - start); }
    private static void equal(List<String> actual, String... expected) { check(actual.equals(Arrays.asList(expected)), actual + " expected " + Arrays.toString(expected)); }
    private static synchronized void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
        checks++;
    }
}
