package com.htc.vr.sdk;

import android.view.LayoutInflater;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import com.htc.vr.R;
import com.htc.vr.sdk.overlay.OverlaySurfaces;
import com.htc.vr.sdk.overlay.VROverlayError;

/** Method donor only: constructors, remaining methods and native code are preserved. */
public class VROverlayServiceManagerDecorator {
    private LayoutInflater mInflater;
    private ProgressBar mGaze;
    private ViewGroup mGazeRoot;
    private Surface[] mGazeSurfaces;
    private boolean mGazeSwapBuffer;
    private OverlaySurfaces mOverlaySurfaces;

    private void initGaze() {
        // triggerClientReady already uses this monitor while replacing the owner.
        // drawGaze and releaseOverlaySurfaces now use it for the whole use/retire interval.
        synchronized (this) {
            Surface[] previous = mGazeSurfaces;
            mGazeSurfaces = null;
            OverlaySurfaces owner = mOverlaySurfaces;
            if (owner == null || mInflater == null) return;

            View inflated = mInflater.inflate(R.layout.gaze_layout, null);
            if (!(inflated instanceof ViewGroup)) return;
            ViewGroup root = (ViewGroup) inflated;
            View progressView = root.findViewById(R.id.progressBar1);
            if (!(progressView instanceof ProgressBar)) return;
            ProgressBar gaze = (ProgressBar) progressView;
            gaze.setScaleX(1.0f);
            gaze.setScaleY(1.0f);
            gaze.setProgress(0);

            // Refresh even when the ID already exists: cached handles can belong
            // to a previous owner, and a partial surface creation is not drawable.
            root.measure(View.MeasureSpec.makeMeasureSpec(0, 0),
                    View.MeasureSpec.makeMeasureSpec(0, 0));
            if (!owner.containsId(2)) owner.add(2, false, 2);
            Surface[] surfaces = owner.getSurface(2);
            if (surfaces == null || surfaces.length < 2) return;
            Surface first = surfaces[0];
            Surface second = surfaces[1];
            if (first == null || second == null || !first.isValid() || !second.isValid()) return;
            if (previous == null || previous.length < 2
                    || previous[0] != first || previous[1] != second) {
                owner.setSurfaceSize(2, root.getMeasuredWidth(), root.getMeasuredHeight());
            }

            mGazeRoot = root;
            mGaze = gaze;
            // Own the two-element snapshot; never depend on a mutable returned array.
            mGazeSurfaces = new Surface[] { first, second };
        }
    }

    private void drawGaze(int progress) {
        synchronized (this) {
            OverlaySurfaces owner = mOverlaySurfaces;
            if (owner == null) return;
            // Binder callbacks can remove/regenerate an individual overlay under
            // this owner's monitor without taking the decorator monitor.
            synchronized (owner) {
                initGaze();
                Surface[] surfaces = mGazeSurfaces;
                ViewGroup root = mGazeRoot;
                ProgressBar gaze = mGaze;
                if (surfaces == null || surfaces.length < 2 || root == null || gaze == null) return;
                Surface first = surfaces[0];
                Surface second = surfaces[1];
                if (first == null || second == null || !first.isValid() || !second.isValid()) return;
                Surface target = mGazeSwapBuffer ? second : first;
                gaze.setProgress(progress);
                drawViewOnSurface(target, root);
                mGazeSwapBuffer = !mGazeSwapBuffer;
            }
        }
    }

    public void releaseOverlaySurfaces() {
        OverlaySurfaces retired;
        synchronized (this) {
            // Acquiring this monitor waits for an active draw to finish before
            // quit can queue release of its surfaces. Detach before any cleanup.
            retired = mOverlaySurfaces;
            mOverlaySurfaces = null;
            mGazeSurfaces = null;
            mGazeRoot = null;
            mGaze = null;
            mGazeSwapBuffer = false;
        }
        // Preserve asynchronous SDK cleanup; do not hold the decorator monitor
        // while entering another object's shutdown implementation.
        if (retired != null) retired.quit();
    }

    // Compile-only declaration: the original renderer implementation is retained.
    private VROverlayError drawViewOnSurface(Surface surface, View view) {
        throw new AssertionError("Compile-only method must never be packaged");
    }
}
