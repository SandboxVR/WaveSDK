package android.os;
public class Handler {
    public static volatile boolean failConstructor;
    public static volatile boolean rejectPosts;
    public static volatile Runnable beforePost;
    private final Looper looper;
    public Handler(Looper looper) {
        if (failConstructor) throw new IllegalStateException("handler allocation failed");
        this.looper = looper;
    }
    public final boolean post(Runnable work) {
        if (beforePost != null) beforePost.run();
        if (rejectPosts || looper.thread.quitting) return false;
        return looper.thread.queue.offer(work);
    }
}
