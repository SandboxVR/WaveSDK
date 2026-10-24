import android.os.HandlerThread;
import android.os.Looper;
import com.htc.vr.sdk.util.CustomizeHandler;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** Runs in an isolated shell ART process; never binds the compositor or loads Wave JNI. */
public final class AndroidSmoke {
    private static int assertions;
    private static void check(boolean value, String message) {
        assertions++;
        if (!value) throw new AssertionError(message);
    }
    public static void main(String[] args) throws Exception {
        Looper.prepareMainLooper();
        // Resolve patched runtime classes without initializing the VR platform/native bridge.
        String[] names = {
            "com.htc.vr.sdk.util.CustomizeHandler",
            "com.htc.vr.sdk.util.CustomizeHandler$SyncRunnable",
            "com.htc.vr.sdk.util.CustomizeHandler$ShutdownRunnable",
            "com.htc.vr.sdk.overlay.OverlaySurfaces",
            "com.htc.vr.core.VRCompositorManager$Listener$binder$1",
            "com.htc.vr.sdk.VRActivityDelegate$1",
            "com.htc.vr.core.ClientSdkP1Compositor"
        };
        for (String name : names) {
            Class<?> c = Class.forName(name, false, AndroidSmoke.class.getClassLoader());
            check(c.getDeclaredMethods().length > 0, "ART method resolution: " + name);
        }
        HandlerThread thread = new HandlerThread("WaveSdkP1Smoke");
        thread.start();
        CustomizeHandler handler = new CustomizeHandler(thread.getLooper());
        try {
            AtomicInteger count = new AtomicInteger();
            handler.runOnThreadSync(count::incrementAndGet);
            check(count.get() == 1, "sync dispatch executes");
            RuntimeException marker = new RuntimeException("expected smoke failure");
            try {
                handler.runOnThreadSync(() -> { throw marker; });
                throw new AssertionError("exception was lost");
            } catch (RuntimeException caught) { check(caught == marker, "original failure propagated"); }
            CountDownLatch started = new CountDownLatch(1), release = new CountDownLatch(1);
            handler.runOnThreadAsync(() -> {
                started.countDown();
                try { release.await(1, TimeUnit.SECONDS); }
                catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
            });
            check(started.await(1, TimeUnit.SECONDS), "blocker started");
            long start = System.nanoTime();
            try {
                handler.runOnThreadSync(count::incrementAndGet, 20, 0);
                throw new AssertionError("queued work did not time out");
            } catch (IllegalStateException expected) {
                check(TimeUnit.NANOSECONDS.toMillis(System.nanoTime()-start) < 1000, "bounded actual ART wait");
            } finally { release.countDown(); }
            handler.runOnThreadSync(() -> { });
            check(count.get() == 1, "cancelled work never executes");
            handler.quit();
            handler.quit();
            thread.join(1000);
            check(!thread.isAlive(), "worker exits after cleanup");
            start = System.nanoTime();
            try {
                handler.runOnThreadSync(count::incrementAndGet);
                throw new AssertionError("closed handler accepted work");
            } catch (RejectedExecutionException expected) {
                check(TimeUnit.NANOSECONDS.toMillis(System.nanoTime()-start) < 100, "closed handler rejects promptly");
            }
            CustomizeHandler main = new CustomizeHandler(Looper.getMainLooper());
            main.quit(); // Must never call quitSafely on Android's non-quittable main looper.
            check(Looper.myLooper() == Looper.getMainLooper(), "main looper preserved");
            System.out.println("PASS Android ART smoke: " + assertions + " assertions; no Wave service/JNI calls");
        } finally {
            thread.quitSafely();
            thread.join(2000);
        }
    }
}
