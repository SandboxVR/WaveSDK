package android.opengl;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
public final class EGL14 {
    public static final EGLDisplay EGL_NO_DISPLAY = new EGLDisplay();
    public static final EGLContext EGL_NO_CONTEXT = new EGLContext();
    public static final EGLSurface EGL_NO_SURFACE = new EGLSurface();
    public static final ThreadLocal<EGLContext> current = new ThreadLocal<EGLContext>();
    public static final List<Thread> destroys = new CopyOnWriteArrayList<Thread>();
    public static EGLContext eglGetCurrentContext() {
        EGLContext context = current.get();
        return context == null ? EGL_NO_CONTEXT : context;
    }
    public static boolean eglMakeCurrent(EGLDisplay d, EGLSurface a, EGLSurface b, EGLContext c) {
        current.set(c); return true;
    }
    public static boolean eglDestroyContext(EGLDisplay d, EGLContext c) {
        destroys.add(Thread.currentThread()); return true;
    }
}
