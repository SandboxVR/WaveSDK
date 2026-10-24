package com.htc.vr.core.jni;
import android.os.Bundle;
import java.io.Serializable;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;

/** Compile signatures and instrumented HOST ownership model, never packaged. */
public final class SharedMemory implements AutoCloseable {
    public static final Companion Companion = new Companion();
    public static final List<SharedMemory> allocations = new ArrayList<SharedMemory>();
    private static final IdentityHashMap<ByteBuffer, SharedMemory> owners =
            new IdentityHashMap<ByteBuffer, SharedMemory>();
    public static volatile boolean failCreate;
    public static volatile boolean failMap;
    public static volatile Runnable beforeBundle;
    public final ByteBuffer mapping;
    public volatile boolean closed;
    public int closeCalls;
    public int writes;
    private SharedMemory(long bytes) {
        mapping = ByteBuffer.allocate((int) bytes);
        synchronized (allocations) { allocations.add(this); }
        synchronized (owners) { owners.put(mapping, this); }
    }
    public ByteBuffer mapReadWrite() {
        checkOpen();
        if (failMap) throw new IllegalStateException("injected map failure");
        return mapping;
    }
    public Bundle toBundle() {
        checkOpen();
        Runnable hook = beforeBundle;
        if (hook != null) hook.run();
        checkOpen();
        Bundle bundle = new Bundle();
        bundle.putSerializable("fd", new Descriptor());
        return bundle;
    }
    public void checkOpen() {
        if (closed) throw new AssertionError("use after native unmap");
    }
    @Override public void close() { closeCalls++; closed = true; }
    public static SharedMemory owner(ByteBuffer buffer) {
        synchronized (owners) { return owners.get(buffer); }
    }
    public static final class Companion {
        public SharedMemory create(long size, String name) {
            if (failCreate) throw new IllegalStateException("injected create failure");
            return new SharedMemory(size);
        }
    }
    /** Models the independent duplicate returned by Android PFD.fromFd. */
    public static final class Descriptor implements Serializable {
        public boolean closed;
        public void checkOpen() {
            if (closed) throw new AssertionError("duplicate closed before serialization");
        }
    }
}
