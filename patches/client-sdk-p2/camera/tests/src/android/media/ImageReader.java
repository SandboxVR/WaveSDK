package android.media;
import android.os.Handler;
import android.view.Surface;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicBoolean;
public class ImageReader {
    public interface OnImageAvailableListener { void onImageAvailable(ImageReader reader); }
    public static final List<ImageReader> created = new CopyOnWriteArrayList<ImageReader>();
    public static volatile boolean failNew;
    public static volatile boolean failListener;
    public static volatile boolean failSurface;
    public static volatile boolean failDetach;
    public static volatile boolean failClose;
    public static volatile Runnable beforeNew;
    public volatile OnImageAvailableListener listener;
    public volatile Handler handler;
    public volatile int closes;
    public volatile int detaches;
    public final AtomicBoolean hasImage = new AtomicBoolean();
    public static ImageReader newInstance(int width, int height, int format, int maxImages) {
        if (beforeNew != null) beforeNew.run();
        if (failNew) throw new IllegalStateException("reader allocation failed");
        if (width <= 0 || height <= 0 || format != 35 || maxImages != 2) throw new AssertionError("reader config");
        ImageReader reader = new ImageReader(); created.add(reader); return reader;
    }
    public void setOnImageAvailableListener(OnImageAvailableListener listener, Handler handler) {
        if (listener == null) { detaches++; if (failDetach) throw new IllegalStateException("detach failed"); }
        else if (failListener) throw new IllegalStateException("listener failed");
        this.listener = listener; this.handler = handler;
    }
    public Surface getSurface() {
        if (failSurface) throw new IllegalStateException("surface failed");
        return new Surface();
    }
    public void close() { closes++; if (failClose) throw new IllegalStateException("close failed"); }
}
