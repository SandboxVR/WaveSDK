package android.os;
public class Handler {
    private final Looper looper;
    public Handler(Looper looper) { this.looper = looper; }
    public final Looper getLooper() { return looper; }
    public final boolean post(Runnable work) { return looper.post(work, false); }
    public final boolean postAtFrontOfQueue(Runnable work) { return looper.post(work, true); }
    public final boolean postDelayed(Runnable work, long delay) { return looper.post(work, false); }
    public final void removeCallbacks(Runnable work) { looper.queue.removeIf(item -> item == work); }
}
