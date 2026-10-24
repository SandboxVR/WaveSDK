import android.opengl.EGL14;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.os.Looper;
import android.util.Log;
import com.htc.vr.sdk.util.CustomizeHandler;
import java.lang.reflect.Field;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

/** Executes the real donor/grafted methods against explicit, bounded queue stubs. */
public final class DispatchRegression {
    private static int checks;
    private static final List<Throwable> threadFailures = new CopyOnWriteArrayList<Throwable>();
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
        checks++;
    }
    private static void await(CountDownLatch latch) {
        try {
            if (!latch.await(8, TimeUnit.SECONDS)) throw new AssertionError("latch deadline");
        } catch (InterruptedException failure) { throw new AssertionError(failure); }
    }
    private static void join(Thread thread) throws Exception {
        thread.join(1800);
        check(!thread.isAlive(), "caller must finish within host deadline");
    }
    private static Throwable expect(Class<? extends Throwable> type, Runnable action) {
        try { action.run(); } catch (Throwable failure) {
            check(type.isInstance(failure), "wrong failure: " + failure);
            return failure;
        }
        throw new AssertionError("Expected " + type.getName());
    }
    private static void waitQueued(Looper looper) throws Exception {
        long deadline = System.nanoTime() + 1000000000L;
        while (looper.queue.isEmpty() && System.nanoTime() < deadline) Thread.sleep(1);
        check(!looper.queue.isEmpty(), "queued callback visible");
    }
    private static void block(CustomizeHandler handler, CountDownLatch entered, CountDownLatch release) {
        handler.post(() -> { entered.countDown(); await(release); });
        await(entered);
    }
    private static void set(Object object, String name, Object value) throws Exception {
        Field field = CustomizeHandler.class.getDeclaredField(name);
        field.setAccessible(true); field.set(object, value);
    }

    private static void successAndFailure() throws Exception {
        Looper looper = new Looper(); CustomizeHandler handler = new CustomizeHandler(looper);
        AtomicInteger runs = new AtomicInteger();
        for (int i = 0; i < 250; i++) handler.runOnThreadSync(runs::incrementAndGet);
        check(runs.get() == 250, "completion before/after await cannot lose notification");
        RuntimeException runtime = new IllegalArgumentException("work failure");
        check(expect(RuntimeException.class, () -> handler.runOnThreadSync(() -> { throw runtime; })) == runtime,
                "runtime failure identity propagated to caller");
        Error error = new AssertionError("work error");
        check(expect(Error.class, () -> handler.runOnThreadSync(() -> { throw error; })) == error,
                "error identity propagated to caller");
        handler.runOnThreadSync(() -> handler.runOnThreadSync(runs::incrementAndGet));
        check(runs.get() == 251, "same-thread nested dispatch completes inline");
        check(looper.uncaught == null, "synchronous failures do not kill worker");
        handler.quit(); looper.awaitExit();
    }

    private static void rejectedAndClosed() throws Exception {
        Looper looper = new Looper(); CustomizeHandler handler = new CustomizeHandler(looper);
        looper.quitSafely(); looper.awaitExit();
        long start = System.nanoTime();
        expect(RejectedExecutionException.class, () -> handler.runOnThreadSync(() -> { }));
        check(System.nanoTime() - start < 500000000L, "rejected synchronous post fails promptly");
        expect(RejectedExecutionException.class, () -> handler.runOnThreadAsync(() -> { }));
        expect(RejectedExecutionException.class, () -> handler.runOnThreadAsync(() -> { }, 1));
        expect(RejectedExecutionException.class, () -> handler.runOnUiThreadImmediateAsync(() -> { }));
        expect(RejectedExecutionException.class, handler::quit);
        handler.quit();
        expect(RejectedExecutionException.class, () -> handler.runOnThreadSync(() -> { }, 10, 0));

        CustomizeHandler main = new CustomizeHandler(Looper.getMainLooper());
        main.quit(); main.quit();
        check(Looper.getMainLooper().quitCalls == 0, "closing handler never quits shared main looper");
        expect(RejectedExecutionException.class, () -> main.runOnThreadSync(() -> { }));
        expect(RejectedExecutionException.class, () -> main.runOnThreadSync(() -> { }, 10, 0));
        expect(RejectedExecutionException.class, () -> main.runOnThreadAsync(() -> { }));
        expect(RejectedExecutionException.class, () -> main.runOnThreadAsync(() -> { }, 1));
        expect(RejectedExecutionException.class, () -> main.runOnUiThreadImmediateAsync(() -> { }));
    }

    private static void queuedCancellationAndSpuriousWake() throws Exception {
        Looper looper = new Looper(); CustomizeHandler handler = new CustomizeHandler(looper);
        CountDownLatch entered = new CountDownLatch(1), release = new CountDownLatch(1);
        block(handler, entered, release);
        AtomicInteger mutations = new AtomicInteger();
        Throwable timeout = expect(IllegalStateException.class,
                () -> handler.runOnThreadSync(mutations::incrementAndGet, 35, 0));
        check(timeout.getCause() instanceof TimeoutException, "timeout has explicit cause");
        release.countDown(); handler.runOnThreadSync(() -> { });
        check(mutations.get() == 0, "timed-out queued work never executes later");

        CountDownLatch entered2 = new CountDownLatch(1), release2 = new CountDownLatch(1);
        block(handler, entered2, release2);
        AtomicReference<Throwable> result = new AtomicReference<Throwable>();
        Thread caller = new Thread(() -> {
            try { handler.runOnThreadSync(mutations::incrementAndGet, 1000, 0); }
            catch (Throwable failure) { result.set(failure); }
        });
        caller.start(); waitQueued(looper);
        Runnable queued = looper.queue.peek();
        for (int i = 0; i < 8; i++) {
            synchronized (queued) { queued.notifyAll(); }
            Thread.sleep(5);
            check(caller.isAlive(), "spurious notification does not satisfy completion predicate");
        }
        release2.countDown(); join(caller);
        check(result.get() == null && mutations.get() == 1, "completed work returns exactly once");

        CountDownLatch entered3 = new CountDownLatch(1), release3 = new CountDownLatch(1);
        block(handler, entered3, release3);
        AtomicBoolean interruptPreserved = new AtomicBoolean();
        Thread interrupted = new Thread(() -> {
            expect(IllegalStateException.class, () -> handler.runOnThreadSync(mutations::incrementAndGet));
            interruptPreserved.set(Thread.currentThread().isInterrupted());
        });
        interrupted.start(); waitQueued(looper); interrupted.interrupt(); join(interrupted);
        check(interruptPreserved.get(), "interrupt status restored");
        release3.countDown(); handler.runOnThreadSync(() -> { });
        check(mutations.get() == 1, "interrupted queued work never executes later");
        handler.quit(); looper.awaitExit();
    }

    private static void startedWorkHasBoundedCaller() throws Exception {
        Looper looper = new Looper(); CustomizeHandler handler = new CustomizeHandler(looper);
        CountDownLatch entered = new CountDownLatch(1), release = new CountDownLatch(1);
        RuntimeException late = new IllegalStateException("failure after waiter departed");
        Thread caller = new Thread(() -> expect(IllegalStateException.class, () ->
                handler.runOnThreadSync(() -> { entered.countDown(); await(release); throw late; }, 60, 0)));
        caller.start(); await(entered); join(caller);
        check(!looper.quitting, "deadline releases caller without killing running worker");
        release.countDown(); handler.runOnThreadSync(() -> { });
        check(Log.failures.contains(late), "failure after timeout remains observable");
        handler.quit(); looper.awaitExit();
    }

    private static void timeoutValidation() throws Exception {
        Looper looper = new Looper(); CustomizeHandler handler = new CustomizeHandler(looper);
        expect(IllegalArgumentException.class, () -> handler.runOnThreadSync(() -> { }, -1, 0));
        expect(IllegalArgumentException.class, () -> handler.runOnThreadSync(() -> { }, 1, 1000000));
        expect(IllegalArgumentException.class, () -> handler.runOnThreadSync(() -> { }, 1, -1));
        handler.runOnThreadSync(() -> { try { Thread.sleep(25); } catch (InterruptedException e) { } }, Long.MAX_VALUE, 999999);
        handler.runOnThreadSync(() -> { }, 0, 0);
        handler.quit(); looper.awaitExit();
    }

    private static void shutdownOrderAndRace() throws Exception {
        EGL14.destroys.clear();
        Looper looper = new Looper(); CustomizeHandler handler = new CustomizeHandler(looper);
        set(handler, "mEGLDisplay", new EGLDisplay());
        EGLContext context = new EGLContext(); set(handler, "mEGLContext", context);
        AtomicBoolean beforeDestroy = new AtomicBoolean();
        handler.runOnThreadSync(() -> {
            EGL14.current.set(context);
            handler.post(() -> beforeDestroy.set(EGL14.destroys.isEmpty()));
            handler.quit();
        });
        looper.awaitExit();
        check(beforeDestroy.get(), "target-thread quit queues cleanup after already accepted work");
        check(EGL14.destroys.size() == 1 && EGL14.destroys.get(0) == looper.getThread(),
                "EGL destruction happens exactly once on owner thread");
        handler.quit(); check(looper.quitCalls == 1, "quit is idempotent");
        expect(RejectedExecutionException.class, () -> handler.runOnThreadAsync(() -> { }));

        for (int trial = 0; trial < 30; trial++) {
            Looper queue = new Looper(); CustomizeHandler target = new CustomizeHandler(queue);
            CountDownLatch start = new CountDownLatch(1);
            AtomicReference<Throwable> unexpected = new AtomicReference<Throwable>();
            Thread producer = new Thread(() -> {
                await(start);
                try { target.runOnThreadSync(() -> { }); }
                catch (RejectedExecutionException expected) { }
                catch (Throwable failure) { unexpected.set(failure); }
            });
            Thread closer = new Thread(() -> {
                await(start);
                try { target.quit(); } catch (Throwable failure) { unexpected.set(failure); }
            });
            producer.start(); closer.start(); start.countDown(); join(producer); join(closer); queue.awaitExit();
            check(unexpected.get() == null, "racing submit/quit either completes or rejects");
        }
    }

    private static void finiteDefaultAndShutdownTimeout() throws Exception {
        Looper workQueue = new Looper(), shutdownQueue = new Looper();
        CustomizeHandler workHandler = new CustomizeHandler(workQueue), shutdownHandler = new CustomizeHandler(shutdownQueue);
        CountDownLatch entered = new CountDownLatch(1), release = new CountDownLatch(1);
        CountDownLatch entered2 = new CountDownLatch(1), release2 = new CountDownLatch(1);
        block(workHandler, entered, release); block(shutdownHandler, entered2, release2);
        AtomicInteger mutations = new AtomicInteger();
        long start = System.nanoTime();
        AtomicReference<Throwable> workFailure = new AtomicReference<Throwable>(), shutdownFailure = new AtomicReference<Throwable>();
        Thread caller = new Thread(() -> {
            try { workHandler.runOnThreadSync(mutations::incrementAndGet); }
            catch (Throwable failure) { workFailure.set(failure); }
        });
        Thread closer = new Thread(() -> {
            try { shutdownHandler.quit(); } catch (Throwable failure) { shutdownFailure.set(failure); }
        });
        caller.start(); closer.start(); caller.join(6500); closer.join(1500);
        check(!caller.isAlive() && !closer.isAlive(), "default dispatch and shutdown callers are bounded");
        check(workFailure.get() instanceof IllegalStateException && shutdownFailure.get() instanceof IllegalStateException,
                "default and shutdown deadline failures are explicit");
        long elapsed = System.nanoTime() - start;
        check(elapsed >= 4500000000L && elapsed < 7500000000L, "finite five-second default deadline");
        release.countDown(); release2.countDown();
        workHandler.runOnThreadSync(() -> { }); workHandler.quit();
        workQueue.awaitExit(); shutdownQueue.awaitExit();
        check(mutations.get() == 0, "default-deadline queued work canceled");
        check(shutdownQueue.quitCalls == 1, "shutdown cleanup survives caller timeout");
    }

    public static void main(String[] args) throws Exception {
        Thread.setDefaultUncaughtExceptionHandler((thread, failure) -> threadFailures.add(failure));
        Looper.getMainLooper();
        System.out.println("CustomizeHandler origin: " + CustomizeHandler.class.getProtectionDomain().getCodeSource().getLocation());
        successAndFailure(); rejectedAndClosed(); queuedCancellationAndSpuriousWake();
        startedWorkHasBoundedCaller(); timeoutValidation(); shutdownOrderAndRace(); finiteDefaultAndShutdownTimeout();
        check(threadFailures.isEmpty(), "all caller thread assertions passed: " + threadFailures);
        System.out.println("PASS dispatch: " + checks + " assertions");
    }
}
