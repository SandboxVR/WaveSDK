import android.view.*;
import android.widget.ProgressBar;
import com.htc.vr.sdk.VROverlayServiceManagerDecorator;
import com.htc.vr.sdk.overlay.OverlaySurfaces;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicReference;

public class GazeRegression {
    static int checks;
    static void check(boolean value, String message) {
        checks++;
        if (!value) throw new AssertionError(message);
    }
    static Surface[] pair() { return new Surface[] {new Surface(), new Surface()}; }
    static LayoutInflater inflater() {
        ViewGroup root = new ViewGroup();
        root.child = new ProgressBar();
        return new LayoutInflater(root);
    }
    static VROverlayServiceManagerDecorator fixture(OverlaySurfaces owner) {
        return new VROverlayServiceManagerDecorator(inflater(), owner);
    }
    static void await(CountDownLatch latch) {
        try { if (!latch.await(3, TimeUnit.SECONDS)) throw new AssertionError("timed out"); }
        catch (InterruptedException e) { Thread.currentThread().interrupt(); throw new AssertionError(e); }
    }
    static void missingResources() {
        Surface invalid = new Surface(); invalid.valid = false;
        Surface[][] cases = {null, new Surface[0], new Surface[] {new Surface()},
                new Surface[] {null, new Surface()}, new Surface[] {new Surface(), null},
                new Surface[] {invalid, new Surface()}, new Surface[] {new Surface(), invalid}};
        for (Surface[] surfaces : cases) {
            OverlaySurfaces owner = new OverlaySurfaces(surfaces);
            VROverlayServiceManagerDecorator d = fixture(owner);
            d.draw(45);
            check(d.draws == 0, "missing/partial/invalid surfaces must skip draw");
            check(owner.sizes == 0, "invalid pair must not resize");
            owner.surfaces = pair();
            d.draw(45);
            check(d.draws == 1, "retry must reacquire an existing ID after partial creation");
            check(owner.adds == 1, "retry must not duplicate overlay allocation");
        }
        VROverlayServiceManagerDecorator d = fixture(null);
        d.draw(50); check(d.draws == 0, "missing owner");
        d = new VROverlayServiceManagerDecorator(null, new OverlaySurfaces(pair()));
        d.draw(50); check(d.draws == 0, "missing inflater");
        ViewGroup wrongChild = new ViewGroup(); wrongChild.child = new View();
        for (View root : new View[] {null, new View(), new ViewGroup(), wrongChild}) {
            d = new VROverlayServiceManagerDecorator(new LayoutInflater(root), new OverlaySurfaces(pair()));
            d.draw(50); check(d.draws == 0, "missing or wrong gaze views");
        }
    }
    static void normalDrawingAndReplacement() {
        OverlaySurfaces owner = new OverlaySurfaces(pair());
        owner.contains = true;
        VROverlayServiceManagerDecorator d = fixture(owner);
        d.staleSurfaces(new Surface[] {null});
        d.draw(25); check(d.lastTarget == owner.surfaces[0], "refresh stale array for existing ID");
        d.draw(50); check(d.lastTarget == owner.surfaces[1], "double buffer preserved");
        check(owner.adds == 0, "existing ID must not allocate again");
        check(owner.sizes == 1, "stable buffers must not be resized on every animation frame");
        d.releaseOverlaySurfaces();
        check(d.detached() && owner.quits == 1, "release detaches and quits once");
        d.draw(75); check(d.draws == 2, "late draw must not recreate retired owner");
        d.releaseOverlaySurfaces(); check(owner.quits == 1, "repeated release");
        OverlaySurfaces next = new OverlaySurfaces(pair());
        d.replaceOwner(next); d.draw(10);
        check(d.lastTarget == next.surfaces[0], "new owner resets gaze swap state");
    }
    static void teardownDuringDraw() throws Exception {
        for (int iteration = 0; iteration < 30; iteration++) {
            OverlaySurfaces owner = new OverlaySurfaces(pair());
            VROverlayServiceManagerDecorator d = fixture(owner);
            CountDownLatch drawing = new CountDownLatch(1), finishDraw = new CountDownLatch(1), releasing = new CountDownLatch(1);
            AtomicReference<Throwable> failure = new AtomicReference<>();
            d.duringDraw = () -> { drawing.countDown(); await(finishDraw); };
            Thread draw = new Thread(() -> { try { d.draw(10); } catch (Throwable t) { failure.set(t); } });
            Thread release = new Thread(() -> { releasing.countDown(); try { d.releaseOverlaySurfaces(); } catch (Throwable t) { failure.set(t); } });
            draw.start(); await(drawing); release.start(); await(releasing);
            long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(3);
            while (release.getState() != Thread.State.BLOCKED && release.isAlive() && System.nanoTime() < deadline) Thread.yield();
            check(release.getState() == Thread.State.BLOCKED, "release waits for active draw monitor");
            check(owner.quits == 0 && owner.surfaces[0].isValid(), "active draw retains valid surface");
            finishDraw.countDown(); draw.join(3000); release.join(3000);
            check(!draw.isAlive() && !release.isAlive() && failure.get() == null, "interleaving completes without race");
            check(owner.quits == 1 && d.detached(), "cleanup occurs after draw");
        }
    }
    static void failuresAndSnapshot() {
        OverlaySurfaces owner = new OverlaySurfaces(pair());
        VROverlayServiceManagerDecorator d = fixture(owner);
        Surface expected = owner.surfaces[0];
        d.duringDraw = () -> { owner.surfaces[0] = null; owner.surfaces[1] = null; };
        d.draw(20); check(d.lastTarget == expected, "draw uses owned snapshot even if returned array changes");
        OverlaySurfaces throwingOwner = new OverlaySurfaces(pair());
        VROverlayServiceManagerDecorator failing = fixture(throwingOwner);
        RuntimeException renderFailure = new RuntimeException("renderer failure");
        failing.duringDraw = () -> { throw renderFailure; };
        try { failing.draw(20); throw new AssertionError("render failure swallowed"); }
        catch (RuntimeException actual) { check(actual == renderFailure, "renderer errors preserve identity"); }
        failing.releaseOverlaySurfaces(); check(failing.detached() && throwingOwner.quits == 1, "failed draw releases monitor");
        OverlaySurfaces cleanup = new OverlaySurfaces(pair());
        cleanup.cleanupFailure = new RuntimeException("quit failure");
        VROverlayServiceManagerDecorator closing = fixture(cleanup);
        closing.draw(30);
        try { closing.releaseOverlaySurfaces(); throw new AssertionError("cleanup failure swallowed"); }
        catch (RuntimeException actual) { check(actual == cleanup.cleanupFailure, "cleanup errors preserve identity"); }
        check(closing.detached(), "failed cleanup still detaches all state");
        closing.releaseOverlaySurfaces(); check(cleanup.quits == 1, "failed cleanup is not retried through stale owner");
    }
    static void newOwnerDuringRetirement() throws Exception {
        OverlaySurfaces old = new OverlaySurfaces(pair());
        VROverlayServiceManagerDecorator d = fixture(old);
        d.draw(10);
        CountDownLatch quitting = new CountDownLatch(1), finishQuit = new CountDownLatch(1);
        old.duringQuit = () -> { quitting.countDown(); await(finishQuit); };
        AtomicReference<Throwable> failure = new AtomicReference<>();
        Thread release = new Thread(() -> { try { d.releaseOverlaySurfaces(); } catch (Throwable t) { failure.set(t); } });
        release.start(); await(quitting);
        OverlaySurfaces replacement = new OverlaySurfaces(pair());
        Thread ready = new Thread(() -> { try { d.replaceOwner(replacement); d.draw(20); } catch (Throwable t) { failure.set(t); } });
        ready.start(); ready.join(1000);
        check(!ready.isAlive(), "new owner does not wait for retired owner's cleanup");
        check(d.lastTarget == replacement.surfaces[0], "replacement uses its own buffers");
        finishQuit.countDown(); release.join(3000);
        check(!release.isAlive() && failure.get() == null, "retirement and replacement finish");
        check(replacement.quits == 0 && replacement.surfaces[0].isValid(), "old cleanup never releases replacement");
        d.releaseOverlaySurfaces(); check(replacement.quits == 1, "replacement remains independently releasable");
    }
    static void individualRemovalDuringDraw() throws Exception {
        OverlaySurfaces owner = new OverlaySurfaces(pair());
        VROverlayServiceManagerDecorator d = fixture(owner);
        CountDownLatch drawing = new CountDownLatch(1), finishDraw = new CountDownLatch(1), removing = new CountDownLatch(1);
        AtomicReference<Throwable> failure = new AtomicReference<>();
        d.duringDraw = () -> { drawing.countDown(); await(finishDraw); };
        Thread draw = new Thread(() -> { try { d.draw(10); } catch (Throwable t) { failure.set(t); } });
        Thread remove = new Thread(() -> { removing.countDown(); owner.remove(2); });
        draw.start(); await(drawing); remove.start(); await(removing);
        long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(3);
        while (remove.getState() != Thread.State.BLOCKED && remove.isAlive() && System.nanoTime() < deadline) Thread.yield();
        check(remove.getState() == Thread.State.BLOCKED, "individual removal waits on owner monitor");
        check(owner.surfaces[0].isValid(), "individual removal cannot invalidate active draw");
        finishDraw.countDown(); draw.join(3000); remove.join(3000);
        check(!draw.isAlive() && !remove.isAlive() && failure.get() == null, "individual removal finishes after draw");
        d.duringDraw = null; d.draw(20);
        check(d.draws == 1, "removed pair is not reused by later draw");
    }
    public static void main(String[] args) throws Exception {
        missingResources(); normalDrawingAndReplacement(); teardownDuringDraw(); failuresAndSnapshot(); newOwnerDuringRetirement(); individualRemovalDuringDraw();
        System.out.println("Gaze regression passed: " + checks + " assertions");
    }
}
