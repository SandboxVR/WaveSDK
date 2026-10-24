package android.os;
import java.util.concurrent.ConcurrentLinkedQueue;
public class Handler {
    private final Looper looper;
    private final ConcurrentLinkedQueue<Runnable> queue = new ConcurrentLinkedQueue<Runnable>();
    public boolean reject;
    public boolean throwOnPost;
    public Handler(Looper looper) { this.looper = looper; }
    public Looper getLooper() { return looper; }
    public boolean post(Runnable action) {
        if (throwOnPost) throw new IllegalStateException("test post failure");
        if (reject) return false;
        queue.add(action);
        return true;
    }
    public int queued() { return queue.size(); }
    public void pump() {
        Looper.asMain(new Runnable() {
            @Override public void run() {
                Runnable action;
                while ((action = queue.poll()) != null) action.run();
            }
        });
    }
}
