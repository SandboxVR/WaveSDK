package android.os;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
public class HandlerThread extends Thread {
    public static final List<HandlerThread> created = new CopyOnWriteArrayList<HandlerThread>();
    public final LinkedBlockingQueue<Runnable> queue = new LinkedBlockingQueue<Runnable>();
    public volatile boolean quitting;
    public volatile int quitCalls;
    public volatile Throwable uncaught;
    private final Looper looper = new Looper(this);
    public HandlerThread(String name) { super(name); setDaemon(true); created.add(this); }
    public Looper getLooper() { return looper; }
    public boolean quitSafely() { quitCalls++; quitting = true; return true; }
    public void run() {
        try {
            while (!quitting || !queue.isEmpty()) {
                Runnable work = queue.poll(5, TimeUnit.MILLISECONDS);
                if (work != null) try { work.run(); } catch (Throwable failure) { uncaught = failure; }
            }
        } catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
    }
}
