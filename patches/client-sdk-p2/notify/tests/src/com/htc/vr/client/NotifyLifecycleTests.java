package com.htc.vr.client;

import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import com.google.flatbuffers.FlatBufferBuilder;
import com.htc.vr.component.INotifyDeviceProvider;
import com.htc.vr.core.IVRNotifyDeviceClient;
import com.htc.vr.core.jni.LockFreeRingBuffer;
import com.htc.vr.core.jni.SharedBuffer;
import com.htc.vr.core.jni.SharedMemory;
import java.lang.reflect.Field;
import java.util.concurrent.Callable;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import kotlin.Unit;

/** Runs the exact editable donor methods with modeled Android/native dependencies. */
public final class NotifyLifecycleTests {
    private static final AtomicInteger assertions = new AtomicInteger();
    private static void check(boolean condition, String message) {
        assertions.incrementAndGet();
        if (!condition) throw new AssertionError(message);
    }
    private interface Work { void run() throws Exception; }
    private static void expect(Class<? extends Throwable> type, Work action) throws Exception {
        try { action.run(); }
        catch (Throwable failure) {
            check(type.isInstance(failure), "expected " + type + ", got " + failure);
            return;
        }
        throw new AssertionError("expected " + type);
    }
    private static Object field(Object owner, String name) throws Exception {
        Field field = owner.getClass().getDeclaredField(name);
        field.setAccessible(true);
        return field.get(owner);
    }
    private static SharedMemory latest() {
        synchronized (SharedMemory.allocations) {
            return SharedMemory.allocations.get(SharedMemory.allocations.size() - 1);
        }
    }
    private static int allocations() {
        synchronized (SharedMemory.allocations) { return SharedMemory.allocations.size(); }
    }
    private static void allClosed() {
        synchronized (SharedMemory.allocations) {
            for (SharedMemory memory : SharedMemory.allocations) {
                check(memory.closed, "owner left live");
                check(memory.closeCalls == 1, "native destructor called more than once");
            }
        }
    }
    private static void reset() {
        allClosed();
        SharedMemory.allocations.clear();
        SharedMemory.failCreate = false;
        SharedMemory.failMap = false;
        SharedMemory.beforeBundle = null;
        SharedBuffer.failBuild = false;
        LockFreeRingBuffer.failParse = false;
        LockFreeRingBuffer.failWriter = false;
        LockFreeRingBuffer.beforeWrite = null;
        FlatBufferBuilder.beforeString = null;
    }
    private static VRNotifyDeviceProvider provider(Service service) throws Exception {
        VRNotifyDeviceProvider provider = new VRNotifyDeviceProvider(service,
                new CompletableFuture<Unit>());
        service.provider = provider;
        return provider;
    }

    private static final class Service implements INotifyDeviceProvider {
        final Binder binder = new Binder();
        VRNotifyDeviceProvider provider;
        volatile IVRNotifyDeviceClient client;
        Bundle returnedBundle;
        boolean startResult = true;
        boolean registerResult = true;
        boolean throwStart;
        boolean throwRegister;
        boolean runtimeRegister;
        boolean throwUnregister;
        boolean runtimeUnregister;
        boolean throwStop;
        int starts, registers, unregisters, stops;
        int unregisterDevice;
        Runnable onStart, onRegister, onUnregister;
        private void ipcUnlocked() {
            check(!Thread.holdsLock(provider), "IPC holds provider lock");
        }
        @Override public IBinder asBinder() { return binder; }
        @Override public boolean start(IBinder token, Bundle profile) throws RemoteException {
            ipcUnlocked(); starts++;
            if (onStart != null) onStart.run();
            if (throwStart) throw new RemoteException("start");
            return startResult;
        }
        @Override public boolean registerClient(IBinder token, int device,
                IVRNotifyDeviceClient value, Bundle profile) throws RemoteException {
            ipcUnlocked(); registers++; client = value;
            check(!Thread.holdsLock(value), "register IPC holds layer lock");
            returnedBundle = value.getSharedMemory();
            if (onRegister != null) onRegister.run();
            if (throwRegister) throw new RemoteException("register");
            if (runtimeRegister) throw new IllegalArgumentException("register runtime");
            return registerResult;
        }
        @Override public void unregisterClient(IBinder token, int device,
                IVRNotifyDeviceClient value) throws RemoteException {
            ipcUnlocked(); unregisters++; unregisterDevice = device;
            check(!Thread.holdsLock(value), "unregister IPC holds layer lock");
            if (onUnregister != null) onUnregister.run();
            if (throwUnregister) throw new RemoteException("unregister");
            if (runtimeUnregister) throw new IllegalArgumentException("unregister runtime");
        }
        @Override public void stop(IBinder token) throws RemoteException {
            ipcUnlocked(); stops++;
            if (throwStop) throw new RemoteException("stop");
        }
    }

    private static final class Gate {
        final CountDownLatch entered = new CountDownLatch(1);
        final CountDownLatch release = new CountDownLatch(1);
        final Runnable hook = () -> { entered.countDown(); await(release); };
        void enter() { await(entered); }
        void open() { release.countDown(); }
    }
    private static void await(CountDownLatch latch) {
        try {
            if (!latch.await(3, TimeUnit.SECONDS)) throw new AssertionError("gate timeout");
        } catch (InterruptedException failure) {
            Thread.currentThread().interrupt();
            throw new AssertionError(failure);
        }
    }
    private static final class Task {
        final CountDownLatch done = new CountDownLatch(1);
        final Thread thread;
        volatile Object result;
        volatile Throwable failure;
        Task(Callable<?> action) {
            thread = new Thread(() -> {
                try { result = action.call(); }
                catch (Throwable error) { failure = error; }
                finally { done.countDown(); }
            });
            thread.setDaemon(true);
            thread.start();
        }
        Object finish() {
            await(done);
            if (failure != null) throw new AssertionError("worker failed", failure);
            return result;
        }
        void stillRunning() throws Exception {
            check(!done.await(80, TimeUnit.MILLISECONDS), "close passed an active native user");
        }
    }

    private static void ordinaryLifecycle() throws Exception {
        reset();
        Service service = new Service();
        VRNotifyDeviceProvider p = provider(service);
        check(p.start(7, 64), "start succeeds");
        SharedMemory first = latest();
        check(first.mapping.capacity() == 192, "same two-element aligned layout");
        check(!first.closed, "committed owner stays open");
        IVRNotifyDeviceClient oldCallback = service.client;
        SharedMemory.Descriptor descriptor = (SharedMemory.Descriptor)
                service.returnedBundle.getSerializable("fd");
        p.updateData(7, "payload");
        check(first.writes == 1, "matching update writes");
        p.updateData(8, "mismatch");
        check(first.writes == 1, "mismatched update ignored");
        expect(NullPointerException.class, () -> p.updateData(7, null));
        expect(IllegalStateException.class, () -> p.start(7, 64));
        p.stop(8);
        check(service.unregisterDevice == 7, "unregisters actual owner despite mismatched caller");
        check(first.closed, "stop closes owner");
        check(oldCallback.getSharedMemory() == null, "retired callback returns no memory");
        descriptor.checkOpen();
        check(true, "returned duplicate survives owner close");
        expect(IllegalStateException.class, () -> p.updateData(7, "stopped"));
        expect(IllegalStateException.class, () -> p.stop(7));
        check(p.start(7, 65), "restart succeeds");
        check(latest() != first, "fresh allocation on restart");
        check(oldCallback.getSharedMemory() == null, "old callback cannot expose new allocation");
        check(latest().mapping.capacity() == 208, "preserves eight-byte alignment");
        CompletableFuture<?> stopped = (CompletableFuture<?>) field(p, "mStopped");
        Object activeLayer = field(p, "mLayer");
        stopped.whenComplete((value, failure) -> {
            check(!Thread.holdsLock(p), "future callback holds provider lock");
            check(!Thread.holdsLock(activeLayer), "future callback holds layer lock");
        });
        p.close(); p.close();
        check(stopped.isDone(), "close completes stopped future");
        check(!p.getClosed().isDone(), "original external closed future semantics retained");
        check(service.binder.unlinks == 1, "idempotent unlink");
        check(service.client.getSharedMemory() == null, "closed callback returns null");
        p.stop(7);
        check(service.stops == 1, "terminal stop is harmless and makes no new IPC");
        expect(IllegalStateException.class, () -> p.start(7, 64));
        allClosed();
    }

    private static void failedStarts() throws Exception {
        reset();
        final Service service = new Service();
        service.startResult = false;
        final VRNotifyDeviceProvider p = provider(service);
        check(!p.start(1, 513), "oversized input rejected");
        check(!p.start(1, -1), "negative input rejected");
        check(!p.start(1, Integer.MIN_VALUE), "minimum integer size rejected");
        check(allocations() == 0 && service.starts == 0, "oversize allocates nothing");
        for (int i = 0; i < 100; i++) {
            check(!p.start(1, 64), "direct failed retry");
            check(latest().closed, "failed start frees before next retry");
        }
        check(allocations() == 100, "direct Java retries exercised");
        check(service.registers == 0 && service.stops == 0, "false start needs no remote rollback");
        expect(IllegalStateException.class, () -> p.stop(1));
        p.close();
        allClosed();

        reset();
        Service unusedService = new Service();
        VRNotifyDeviceProvider unused = provider(unusedService);
        unused.close(); unused.close();
        check(allocations() == 0, "close before start is harmless");
        check(unusedService.binder.unlinks == 1, "unused provider unlinks once");
        Service zeroService = new Service();
        VRNotifyDeviceProvider zero = provider(zeroService);
        check(zero.start(1, 0), "zero-sized payload remains accepted");
        zero.close();
    }

    private static void allocationFailures() throws Exception {
        for (int point = 0; point < 5; point++) {
            reset();
            Service service = new Service();
            VRNotifyDeviceProvider p = provider(service);
            if (point == 0) SharedMemory.failCreate = true;
            if (point == 1) SharedMemory.failMap = true;
            if (point == 2) SharedBuffer.failBuild = true;
            if (point == 3) LockFreeRingBuffer.failParse = true;
            if (point == 4) LockFreeRingBuffer.failWriter = true;
            expect(point == 3 ? NullPointerException.class : IllegalStateException.class,
                    () -> p.start(2, 64));
            check(service.starts == 0, "allocation failure never reaches service");
            allClosed();
            SharedMemory.failCreate = false;
            SharedMemory.failMap = false;
            SharedBuffer.failBuild = false;
            LockFreeRingBuffer.failParse = false;
            LockFreeRingBuffer.failWriter = false;
            check(p.start(2, 64), "retry after allocation failure succeeds");
            p.close();
            allClosed();
        }

        reset();
        Service service = new Service();
        VRNotifyDeviceProvider p = provider(service);
        VRNotifyDeviceProvider$Layer layer = new VRNotifyDeviceProvider$Layer(p);
        layer.createShareMemory(64);
        SharedMemory old = latest();
        layer.createShareMemory(64);
        check(old.closed && old.closeCalls == 1, "direct replacement closes previous owner");
        layer.close(); layer.close();
        expect(IllegalStateException.class, () -> layer.createShareMemory(64));
        p.close();
        allClosed();
    }

    private static void remoteFailures() throws Exception {
        for (int point = 0; point < 5; point++) {
            reset();
            Service service = new Service();
            VRNotifyDeviceProvider p = provider(service);
            if (point == 0) service.throwStart = true;
            if (point == 1) service.registerResult = false;
            if (point == 2) service.throwRegister = true;
            if (point == 3) service.runtimeRegister = true;
            if (point == 4) {
                service.throwRegister = true;
                service.throwUnregister = true;
                service.throwStop = true;
            }
            if (point == 3) expect(IllegalArgumentException.class, () -> p.start(3, 64));
            else check(!p.start(3, 64), "remote failed start returns false");
            check(latest().closed, "remote failed start frees owner");
            check(service.stops == 1, "accepted/ambiguous start rolled back");
            check(service.unregisters == (point == 0 ? 0 : 1), "registration rollback attempted");
            check(!(Boolean) field(p, "mStarted"), "failure never publishes started");
            service.throwStart = false;
            service.registerResult = true;
            service.throwRegister = false;
            service.runtimeRegister = false;
            service.throwUnregister = false;
            service.throwStop = false;
            check(p.start(3, 64), "retry after remote failure succeeds");
            p.close();
            allClosed();
        }
        for (int point = 0; point < 3; point++) {
            reset();
            Service service = new Service();
            VRNotifyDeviceProvider p = provider(service);
            check(p.start(4, 64), "stop setup");
            if (point == 0) service.throwUnregister = true;
            if (point == 1) service.throwStop = true;
            if (point == 2) service.runtimeUnregister = true;
            if (point == 2) expect(IllegalArgumentException.class, () -> p.stop(4));
            else p.stop(4);
            check(latest().closed, "failed unregister/stop frees local owner");
            check(service.stops == 1, "stop attempted after unregister failure");
            service.throwUnregister = false;
            service.throwStop = false;
            service.runtimeUnregister = false;
            check(p.start(4, 64), "failed stop allows clean restart");
            p.close();
            allClosed();
        }
    }

    private static void closeDuringStart(boolean registering) throws Exception {
        reset();
        Service service = new Service();
        VRNotifyDeviceProvider p = provider(service);
        Gate gate = new Gate();
        if (registering) service.onRegister = gate.hook;
        else service.onStart = gate.hook;
        Task starting = new Task(() -> p.start(5, 64));
        gate.enter();
        SharedMemory memory = latest();
        expect(IllegalStateException.class, () -> p.start(5, 64));
        expect(IllegalStateException.class, () -> p.stop(5));
        // Must finish independently while the remote call is still blocked.
        Task closing = new Task(() -> { p.close(); return null; });
        closing.finish();
        check(memory.closed, "close frees pending allocation during blocked IPC");
        if (registering) {
            check(service.client.getSharedMemory() == null, "pending closed callback suppressed");
            ((SharedMemory.Descriptor) service.returnedBundle.getSerializable("fd")).checkOpen();
            check(true, "callback duplicate still valid for later Binder serialization");
        }
        gate.open();
        check(Boolean.FALSE.equals(starting.finish()), "closed attempt cannot commit success");
        check(service.stops == 1, "late accepted remote start rolled back");
        check(service.unregisters == (registering ? 1 : 0), "late registration rolled back");
        p.close();
        allClosed();
    }

    private static void closeBeforeAllocation() throws Exception {
        reset();
        Service service = new Service();
        VRNotifyDeviceProvider p = provider(service);
        Object previous = field(p, "mLayer");
        Task starting;
        synchronized (previous) {
            starting = new Task(() -> p.start(5, 64));
            long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(2);
            while (starting.thread.getState() != Thread.State.BLOCKED
                    && System.nanoTime() < deadline) Thread.yield();
            check(starting.thread.getState() == Thread.State.BLOCKED,
                    "start paused before retiring constructor layer");
            p.close();
            check(allocations() == 0, "close won before candidate allocation");
        }
        check(Boolean.FALSE.equals(starting.finish()), "pre-allocation close cancels start");
        check(service.starts == 0, "canceled attempt makes no service call");
        allClosed();
    }

    private static void writeVersusClose() throws Exception {
        reset();
        Service service = new Service();
        VRNotifyDeviceProvider p = provider(service);
        check(p.start(6, 64), "write race setup");
        SharedMemory memory = latest();
        Gate gate = new Gate();
        LockFreeRingBuffer.beforeWrite = gate.hook;
        Task writing = new Task(() -> { p.updateData(6, "race"); return null; });
        gate.enter();
        Task closing = new Task(() -> { p.close(); return null; });
        closing.stillRunning();
        check(!memory.closed, "active write pins native mapping through endWrite");
        gate.open();
        writing.finish(); closing.finish();
        check(memory.writes == 1 && memory.closed, "write then close finishes without UAF");
        allClosed();
    }

    private static void callbackVersusClose() throws Exception {
        reset();
        Service service = new Service();
        VRNotifyDeviceProvider p = provider(service);
        check(p.start(7, 64), "callback race setup");
        SharedMemory memory = latest();
        Gate gate = new Gate();
        SharedMemory.beforeBundle = gate.hook;
        Task callback = new Task(() -> service.client.getSharedMemory());
        gate.enter();
        Task closing = new Task(() -> { p.close(); return null; });
        closing.stillRunning();
        check(!memory.closed, "native owner pinned through descriptor dup");
        gate.open();
        Bundle bundle = (Bundle) callback.finish();
        closing.finish();
        ((SharedMemory.Descriptor) bundle.getSerializable("fd")).checkOpen();
        check(memory.closed, "owner freed after descriptor dup");
        check(service.client.getSharedMemory() == null, "later callback does not touch owner");
        allClosed();
    }

    private static void delayedUpdateDoesNotReachRestart() throws Exception {
        reset();
        Service service = new Service();
        VRNotifyDeviceProvider p = provider(service);
        check(p.start(8, 64), "delayed builder setup");
        SharedMemory old = latest();
        Gate gate = new Gate();
        FlatBufferBuilder.beforeString = gate.hook;
        Task writing = new Task(() -> { p.updateData(8, "old generation"); return null; });
        gate.enter();
        p.stop(8);
        check(p.start(8, 64), "restart while old update is still building");
        SharedMemory replacement = latest();
        gate.open();
        writing.finish();
        check(old.writes == 0 && replacement.writes == 0,
                "delayed old update discarded without writing new generation");
        p.close();
        allClosed();
    }

    private static void closeDuringStopAndDeath() throws Exception {
        reset();
        final Service service = new Service();
        final VRNotifyDeviceProvider p = provider(service);
        check(p.start(9, 64), "stop race setup");
        Gate gate = new Gate();
        service.onUnregister = gate.hook;
        Task stopping = new Task(() -> { p.stop(9); return null; });
        gate.enter();
        expect(IllegalStateException.class, () -> p.start(9, 64));
        p.close();
        check(latest().closed, "close releases owner while unregister is blocked");
        gate.open(); stopping.finish();
        check(service.stops == 1, "stop still finishes remote cleanup");
        allClosed();

        reset();
        Service deathService = new Service();
        VRNotifyDeviceProvider deathProvider = provider(deathService);
        check(deathProvider.start(10, 64), "death setup");
        deathService.binder.die();
        check(deathProvider.getManagerClosed().isDone(), "unchanged death callback signals manager");
        check(latest().closed, "unchanged death callback reaches patched local close");
        check(deathService.client.getSharedMemory() == null, "dead provider callback suppressed");
        deathProvider.close();
        allClosed();
    }

    private static void bridgeCloseOrdering() throws Exception {
        reset();
        Service service = new Service();
        VRNotifyDeviceProvider p = provider(service);
        check(p.start(11, 64), "bridge ordering setup");
        // The shipped module posts these two actions on the same Handler.
        Runnable postedStop = () -> p.stop(11);
        Runnable postedClose = p::close;
        postedStop.run(); postedClose.run();
        check(service.unregisters == 1 && service.stops == 1,
                "bridge's FIFO stop then close performs remote cleanup");
        check(latest().closed, "FIFO close leaves no owner");
        allClosed();

        reset();
        Service dyingService = new Service();
        VRNotifyDeviceProvider dying = provider(dyingService);
        check(dying.start(12, 64), "bridge death race setup");
        Runnable delayedStop = () -> dying.stop(12);
        dyingService.binder.die();
        delayedStop.run();
        check(dyingService.stops == 0 && dyingService.unregisters == 0,
                "queued stop after terminal death is harmless");
        check(latest().closed, "death before queued stop still destroys owner once");
        allClosed();
    }

    public static void main(String[] args) throws Exception {
        ordinaryLifecycle();
        failedStarts();
        allocationFailures();
        remoteFailures();
        closeBeforeAllocation();
        closeDuringStart(false);
        closeDuringStart(true);
        writeVersusClose();
        callbackVersusClose();
        delayedUpdateDoesNotReachRestart();
        closeDuringStopAndDeath();
        bridgeCloseOrdering();
        allClosed();
        System.out.println("PASS: " + assertions.get() + " notify lifecycle assertions");
    }
}
