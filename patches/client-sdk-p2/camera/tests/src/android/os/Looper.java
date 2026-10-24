package android.os;
public final class Looper {
    final HandlerThread thread;
    Looper(HandlerThread thread) { this.thread = thread; }
}
