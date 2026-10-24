package com.htc.vr.core.jni;
import java.nio.ByteBuffer;
import kotlin.Unit;
import kotlin.jvm.functions.Function1;
public final class LockFreeRingBuffer {
    public static final Companion Companion = new Companion();
    public static volatile boolean failParse;
    public static volatile boolean failWriter;
    public static volatile Runnable beforeWrite;
    private final SharedMemory memory;
    private LockFreeRingBuffer(ByteBuffer bytes) { memory = SharedMemory.owner(bytes); }
    public static final class Builder implements SharedBuffer.SectionBuilder {
        final int elementSize;
        public Builder(int count, int elementSize) { this.elementSize = elementSize; }
    }
    public static final class Companion {
        public LockFreeRingBuffer parse(ByteBuffer bytes) {
            if (failParse) return null;
            return new LockFreeRingBuffer(bytes);
        }
    }
    public static final class Writer {
        private final LockFreeRingBuffer ring;
        public Writer(LockFreeRingBuffer ring) {
            if (failWriter) throw new IllegalStateException("injected writer failure");
            this.ring = ring;
        }
        public void write(Function1<? super ByteBuffer, Unit> block) {
            ring.memory.checkOpen();
            Runnable hook = beforeWrite;
            if (hook != null) hook.run();
            ring.memory.checkOpen();
            block.invoke(ring.memory.mapping);
            ring.memory.checkOpen();
            ring.memory.writes++;
        }
    }
}
