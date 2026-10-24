package android.util;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
public final class Log {
    public static final List<Throwable> failures = new CopyOnWriteArrayList<Throwable>();
    public static int d(String tag, String text) { return 0; }
    public static int i(String tag, String text) { return 0; }
    public static int w(String tag, String text) { return 0; }
    public static int e(String tag, String text) { return 0; }
    public static int e(String tag, String text, Throwable error) { failures.add(error); return 0; }
}
