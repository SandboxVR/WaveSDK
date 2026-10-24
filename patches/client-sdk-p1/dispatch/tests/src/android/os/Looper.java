package android.os;
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.TimeUnit;
public final class Looper {
    private static final Looper MAIN = new Looper(Thread.currentThread());
    public final LinkedBlockingDeque<Runnable> queue = new LinkedBlockingDeque<Runnable>();
    public volatile boolean quitting;
    public volatile Throwable uncaught;
    public volatile int quitCalls;
    private final Thread thread;
    private Looper(Thread owner) { thread = owner; }
    public Looper() {
        thread = new Thread(() -> {
            try {
                while (!quitting || !queue.isEmpty()) {
                    Runnable work = queue.poll(10, TimeUnit.MILLISECONDS);
                    if (work != null) {
                        try { work.run(); } catch (Throwable failure) { uncaught = failure; }
                    }
                }
            } catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
        }, "dispatch-test-looper");
        thread.setDaemon(true);
        thread.start();
    }
    public static Looper getMainLooper() { return MAIN; }
    public Thread getThread() { return thread; }
    public synchronized boolean post(Runnable work, boolean front) {
        if (work == null) throw new NullPointerException("work");
        if (quitting) return false;
        if (front) queue.addFirst(work); else queue.addLast(work);
        return true;
    }
    public synchronized void quitSafely() {
        if (this == MAIN) throw new IllegalStateException("Main looper cannot quit");
        quitCalls++;
        quitting = true;
    }
    public void awaitExit() throws InterruptedException {
        thread.join(1500);
        if (thread.isAlive()) throw new AssertionError("looper still running");
    }
}
