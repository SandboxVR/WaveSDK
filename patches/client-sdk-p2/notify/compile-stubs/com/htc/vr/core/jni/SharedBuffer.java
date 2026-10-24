package com.htc.vr.core.jni;
import java.nio.ByteBuffer;
public final class SharedBuffer {
    public static volatile boolean failBuild;
    private final ByteBuffer bytes;
    private SharedBuffer(ByteBuffer bytes) { this.bytes = bytes; }
    public ByteBuffer section(int index) { return bytes; }
    public interface SectionBuilder { }
    public static final class Builder {
        private long size;
        public int addSection(SectionBuilder section) {
            LockFreeRingBuffer.Builder ring = (LockFreeRingBuffer.Builder) section;
            size = 64L + 2L * ((ring.elementSize + 7L) & ~7L);
            return 0;
        }
        public long getTotalSize() { return size; }
        public SharedBuffer build(ByteBuffer bytes) {
            if (failBuild) throw new IllegalStateException("injected build failure");
            return new SharedBuffer(bytes);
        }
    }
}
