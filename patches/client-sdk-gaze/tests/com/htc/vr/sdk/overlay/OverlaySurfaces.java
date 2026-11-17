package com.htc.vr.sdk.overlay;
import android.view.Surface;
public class OverlaySurfaces {
    public Surface[] surfaces;
    public boolean contains;
    public int adds, sizes;
    public volatile int quits;
    public Runnable duringQuit;
    public RuntimeException cleanupFailure;
    public OverlaySurfaces(Surface[] surfaces) { this.surfaces = surfaces; }
    public boolean containsId(int id) { return contains; }
    public void add(int id, boolean protectedContent, int count) {
        if (id != 2 || protectedContent || count != 2) throw new AssertionError("gaze allocation ABI");
        contains = true;
        adds++;
    }
    public Surface[] getSurface(int id) { return surfaces; }
    public void setSurfaceSize(int id, int width, int height) { sizes++; }
    public synchronized void remove(int id) {
        if (surfaces != null) for (Surface surface : surfaces) if (surface != null) surface.valid = false;
        surfaces = null;
        contains = false;
    }
    public void quit() {
        quits++;
        if (duringQuit != null) duringQuit.run();
        if (surfaces != null) for (Surface surface : surfaces) if (surface != null) surface.valid = false;
        if (cleanupFailure != null) throw cleanupFailure;
    }
}
