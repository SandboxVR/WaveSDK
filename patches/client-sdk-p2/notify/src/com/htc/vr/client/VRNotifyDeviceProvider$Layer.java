package com.htc.vr.client;

import android.os.Bundle;
import com.google.flatbuffers.FlatBufferBuilder;
import com.htc.vr.core.IVRNotifyDeviceClient;
import com.htc.vr.core.jni.LockFreeRingBuffer;
import com.htc.vr.core.jni.SharedBuffer;
import com.htc.vr.core.jni.SharedMemory;

/** Method donor: the original Stub superclass, constructor and callbacks remain. */
final class VRNotifyDeviceProvider$Layer extends IVRNotifyDeviceClient.Stub
        implements AutoCloseable {
    final VRNotifyDeviceProvider this$0;
    private SharedMemory mSharedMemory;
    private LockFreeRingBuffer.Writer mWriter;
    private boolean mNotifyClosed;

    // Compile/host constructor only, never grafted.
    public VRNotifyDeviceProvider$Layer(VRNotifyDeviceProvider provider) {
        this$0 = provider;
    }

    public final void createShareMemory(int elementSize) {
        synchronized (this) {
            if (mNotifyClosed) throw new IllegalStateException("Layer is closed");
            SharedMemory previous = mSharedMemory;
            mSharedMemory = null;
            mWriter = null;
            if (previous != null) previous.close();

            SharedBuffer.Builder layout = new SharedBuffer.Builder();
            layout.addSection(new LockFreeRingBuffer.Builder(2, elementSize));
            SharedMemory candidate = SharedMemory.Companion.create(
                    layout.getTotalSize(), "inverseinform");
            try {
                SharedBuffer buffer = layout.build(candidate.mapReadWrite());
                LockFreeRingBuffer ring = LockFreeRingBuffer.Companion.parse(buffer.section(0));
                if (ring == null) throw new NullPointerException("ring");
                LockFreeRingBuffer.Writer writer = new LockFreeRingBuffer.Writer(ring);
                mSharedMemory = candidate;
                mWriter = writer;
                candidate = null;
            } finally {
                if (candidate != null) candidate.close();
            }
        }
    }

    @Override public Bundle getSharedMemory() {
        synchronized (this) {
            // The shipped toBundle calls ParcelFileDescriptor.fromFd, which
            // duplicates the native FD. Keep its native owner alive through dup;
            // Binder may serialize the independently owned result after return.
            if (mNotifyClosed || mSharedMemory == null) return null;
            return mSharedMemory.toBundle();
        }
    }

    public final void writeNotifyData(FlatBufferBuilder builder) {
        synchronized (this) {
            if (!mNotifyClosed && mWriter != null) {
                mWriter.write(new VRNotifyDeviceProvider$updateData$1(builder));
            }
        }
    }

    @Override public void close() {
        synchronized (this) {
            mNotifyClosed = true;
            SharedMemory memory = mSharedMemory;
            mSharedMemory = null;
            mWriter = null;
            if (memory != null) memory.close();
        }
    }

    // Compile/host scaffolding: the original callbacks/getter are not grafted.
    @Override public Bundle call(String method, String arg, Bundle extras) { return null; }
    @Override public void notify(String method, String arg, Bundle extras) { }
    public final LockFreeRingBuffer.Writer getWriter() { return mWriter; }
}
