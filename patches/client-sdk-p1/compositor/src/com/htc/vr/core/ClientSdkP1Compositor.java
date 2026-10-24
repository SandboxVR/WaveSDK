package com.htc.vr.core;

import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;
import java.lang.ref.WeakReference;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Implementation helper; original SDK classes retain their fields and ABI. */
public final class ClientSdkP1Compositor {
    private static final String TAG = "VRCORE_P1";
    private static final long DISCONNECT_TIMEOUT_NANOS = TimeUnit.SECONDS.toNanos(2);
    // Neither State nor its Handler has a strong reference to its weak owner.
    private static final ArrayList<State> STATES = new ArrayList<State>();

    private ClientSdkP1Compositor() {}

    private static State state(VRCompositorManager.Listener listener) {
        synchronized (STATES) {
            for (Iterator<State> it = STATES.iterator(); it.hasNext();) {
                State state = it.next();
                VRCompositorManager.Listener owner = state.owner.get();
                if (owner == null) it.remove();
                else if (owner == listener) return state;
            }
            State state = new State(listener);
            STATES.add(state);
            return state;
        }
    }

    /** Preserve asynchronous availability: registration can itself block the main looper. */
    public static void available(VRCompositorManager.Listener listener,
            IVRCompositor binder, Bundle info) {
        if (listener == null || binder == null) throw new NullPointerException("listener/binder");
        State state = state(listener);
        Event event = new Event(listener, binder, info == null ? null : new Bundle(info), true);
        boolean post;
        synchronized (state) {
            if (state.terminal) return;
            // A connection never delivered to the consumer owns no resources. Retain only
            // the newest pending connection instead of briefly constructing obsolete ones.
            for (Iterator<Event> it = state.queue.iterator(); it.hasNext();) {
                Event pending = it.next();
                if (pending.available) { it.remove(); pending.cancel(); }
            }
            state.queue.add(event);
            post = !state.scheduled;
            state.scheduled = true;
        }
        if (post) post(state);
    }

    public static void unavailable(VRCompositorManager.Listener listener, IVRCompositor binder) {
        if (listener == null || binder == null) throw new NullPointerException("listener/binder");
        State state = state(listener);
        Event event = new Event(listener, binder, null, false);
        boolean post;
        boolean onMain = Looper.myLooper() == state.handler.getLooper();
        synchronized (state) {
            if (state.terminal) return;
            // A loss may overtake an available callback that has not reached the main
            // looper. Cancel that callback even if this loss subsequently times out.
            for (Iterator<Event> it = state.queue.iterator(); it.hasNext();) {
                Event pending = it.next();
                if (pending.available && same(pending.token, event.token)) {
                    it.remove(); pending.cancel();
                }
            }
            if (state.running != null && state.running.available &&
                    same(state.running.token, event.token)) state.running.cancel();
            state.queue.add(event);
            post = !state.scheduled;
            state.scheduled = true;
        }
        if (onMain) {
            // Reentrant callbacks are queued behind the current callback. Waiting here
            // would wait on our own stack; the outer drain will finish them in order.
            drain(state);
            if (!event.done()) return;
        } else {
            if (post) post(state);
            try {
                long remaining = event.deadline - System.nanoTime();
                if (remaining <= 0 || !event.finished.await(remaining, TimeUnit.NANOSECONDS)) {
                    cancel(state, event);
                    Log.w(TAG, "Compositor disconnect wait timed out; pending work cancelled, started callback may finish");
                    return;
                }
            } catch (InterruptedException interrupted) {
                cancel(state, event);
                Thread.currentThread().interrupt();
                Log.w(TAG, "Compositor disconnect interrupted; pending work cancelled, started callback may finish");
                return;
            }
        }
        if (event.failure != null) throw new IllegalStateException("Compositor disconnect callback failed", event.failure);
    }

    /** Terminal SDK policy: stop dispatch before asking the existing delegate to exit. */
    public static void terminate(VRCompositorManager.Listener listener) {
        State state = state(listener);
        synchronized (state) {
            state.terminal = true;
            state.activeToken = null;
            for (Event event : state.queue) event.cancel();
            state.queue.clear();
        }
        VRCompositorManager.Listener.access$setMCompositor$p(listener, null);
    }

    public static boolean isTerminal(VRCompositorManager.Listener listener) {
        State state = state(listener);
        synchronized (state) { return state.terminal; }
    }

    private static void post(State state) {
        boolean accepted = false;
        try { accepted = state.handler.post(new Drain(state)); }
        catch (RuntimeException failure) { Log.e(TAG, "Compositor handler rejected dispatch", failure); }
        if (!accepted) {
            synchronized (state) {
                for (Event event : state.queue) event.cancel();
                state.queue.clear();
                state.scheduled = false;
            }
            Log.w(TAG, "Compositor handler rejected dispatch; queued work cancelled");
        }
    }

    private static void cancel(State state, Event event) {
        synchronized (state) {
            event.cancel();
            state.queue.remove(event);
        }
    }

    private static void drain(State state) {
        synchronized (state) {
            if (state.draining) return;
            state.draining = true;
        }
        for (;;) {
            Event event;
            synchronized (state) {
                event = state.queue.poll();
                if (event == null) {
                    state.scheduled = false;
                    state.draining = false;
                    return;
                }
                state.running = event;
            }
            try {
                if (live(state, event)) apply(state, event);
            } catch (Throwable failure) {
                event.failure = failure;
                // Unknown consumer cleanup after an exception cannot justify creating
                // another owner. Keep this listener terminal and report the failure.
                terminate(event.listener);
                Log.e(TAG, "Compositor callback failed; listener is terminal", failure);
            } finally {
                synchronized (state) { state.running = null; }
                event.finished.countDown();
            }
            if (event.available && event.failure != null) {
                synchronized (state) { state.draining = false; state.scheduled = false; }
                // Original availability Runnable failures were fatal on the main looper.
                // Preserve that behavior after closing this listener to further owners.
                if (event.failure instanceof Error) throw (Error) event.failure;
                if (event.failure instanceof RuntimeException) throw (RuntimeException) event.failure;
                throw new IllegalStateException("Compositor availability callback failed", event.failure);
            }
        }
    }

    private static boolean live(State state, Event event) {
        synchronized (state) {
            return !state.terminal && !event.cancelled &&
                    (event.available || System.nanoTime() - event.deadline < 0);
        }
    }

    private static void apply(State state, Event event) {
        VRCompositorManager.Listener listener = event.listener;
        VRCompositor current = VRCompositorManager.Listener.access$getMCompositor$p(listener);
        IBinder active;
        synchronized (state) { active = state.activeToken == null ? null : state.activeToken.get(); }
        if (!event.available) {
            if (current == null || !same(active, event.token)) return;
            // Clear BEFORE invoking user code, including when that code throws or reenters.
            VRCompositorManager.Listener.access$setMCompositor$p(listener, null);
            synchronized (state) { state.activeToken = null; }
            listener.onCompositorUnavailable(current);
            return;
        }
        if (current != null && same(active, event.token)) return;
        if (current != null) {
            VRCompositorManager.Listener.access$setMCompositor$p(listener, null);
            synchronized (state) { state.activeToken = null; }
            listener.onCompositorUnavailable(current);
            if (!live(state, event)) return;
        }
        VRCompositor compositor = new VRCompositor(event.binder, VRCompositorInfo.Companion.from(event.info));
        if (!live(state, event)) return;
        VRCompositorManager.Listener.access$setMCompositor$p(listener, compositor);
        synchronized (state) { state.activeToken = new WeakReference<IBinder>(event.token); }
        listener.onCompositorAvailable(compositor);
        // A matching loss can arrive while consumer initialization is already running.
        // Its waiter may time out before initialization returns. Complete this started
        // callback by retiring its invalidated owner; never leave it looking healthy,
        // and never depend on executing that canceled queued loss afterwards.
        if (event.cancelled && VRCompositorManager.Listener.access$getMCompositor$p(listener) == compositor) {
            VRCompositorManager.Listener.access$setMCompositor$p(listener, null);
            synchronized (state) { state.activeToken = null; }
            listener.onCompositorUnavailable(compositor);
        }
    }

    private static boolean same(IBinder first, IBinder second) {
        return first == second || (first != null && first.equals(second));
    }

    private static final class Drain implements Runnable {
        private final State state;
        Drain(State state) { this.state = state; }
        @Override public void run() { drain(state); }
    }

    private static final class State {
        final WeakReference<VRCompositorManager.Listener> owner;
        final Handler handler;
        final ArrayDeque<Event> queue = new ArrayDeque<Event>();
        // A local Binder implementation can itself capture its listener. The listener's
        // VRCompositor owns the live Binder; the registry must not add a strong root.
        WeakReference<IBinder> activeToken;
        Event running;
        boolean scheduled;
        boolean draining;
        boolean terminal;
        State(VRCompositorManager.Listener listener) {
            owner = new WeakReference<VRCompositorManager.Listener>(listener);
            handler = VRCompositorManager.Listener.access$getMHandler$p(listener);
        }
    }

    private static final class Event {
        final VRCompositorManager.Listener listener;
        final IVRCompositor binder;
        final IBinder token;
        final Bundle info;
        final boolean available;
        final long deadline;
        final CountDownLatch finished = new CountDownLatch(1);
        volatile boolean cancelled;
        volatile Throwable failure;
        Event(VRCompositorManager.Listener listener, IVRCompositor binder, Bundle info, boolean available) {
            this.listener = listener;
            this.binder = binder;
            this.token = binder.asBinder();
            if (token == null) throw new NullPointerException("binder.asBinder()");
            this.info = info;
            this.available = available;
            this.deadline = available ? 0 : System.nanoTime() + DISCONNECT_TIMEOUT_NANOS;
        }
        void cancel() { cancelled = true; finished.countDown(); }
        boolean done() { return finished.getCount() == 0; }
    }
}
