package com.htc.vr.sdk;
import android.view.*;
import android.widget.ProgressBar;
import com.htc.vr.sdk.overlay.*;

/** Host fixture: Graft replaces only the same three methods as the packaged DEX. */
public class VROverlayServiceManagerDecorator {
    private LayoutInflater mInflater;
    private ProgressBar mGaze;
    private ViewGroup mGazeRoot;
    private Surface[] mGazeSurfaces;
    private boolean mGazeSwapBuffer;
    private OverlaySurfaces mOverlaySurfaces;
    public int draws;
    public Surface lastTarget;
    public Runnable duringDraw;
    public VROverlayServiceManagerDecorator(LayoutInflater inflater, OverlaySurfaces owner) {
        mInflater = inflater;
        mOverlaySurfaces = owner;
    }
    private void initGaze() { throw new AssertionError("original init must be replaced"); }
    private void drawGaze(int progress) { throw new AssertionError("original draw must be replaced"); }
    public void releaseOverlaySurfaces() { throw new AssertionError("original release must be replaced"); }
    private VROverlayError drawViewOnSurface(Surface surface, View root) {
        if (surface == null || !surface.isValid()) throw new AssertionError("invalid draw target");
        lastTarget = surface;
        draws++;
        if (duringDraw != null) duringDraw.run();
        if (!surface.isValid()) throw new AssertionError("surface released while drawing");
        return VROverlayError.None;
    }
    public void draw(int progress) { drawGaze(progress); }
    public synchronized void replaceOwner(OverlaySurfaces owner) { mOverlaySurfaces = owner; }
    public void staleSurfaces(Surface[] surfaces) { mGazeSurfaces = surfaces; }
    public boolean detached() { return mOverlaySurfaces == null && mGazeSurfaces == null && mGazeRoot == null && mGaze == null; }
}
