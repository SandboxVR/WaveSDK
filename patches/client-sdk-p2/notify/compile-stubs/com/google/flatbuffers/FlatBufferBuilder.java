package com.google.flatbuffers;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
/** API signatures plus a small host payload model, never packaged. */
public class FlatBufferBuilder {
    private ByteBuffer payload;
    public static volatile Runnable beforeString;
    public void clear() { payload = null; }
    public int createString(CharSequence text) {
        Runnable hook = beforeString;
        if (hook != null) hook.run();
        payload = ByteBuffer.wrap(text.toString().getBytes(StandardCharsets.UTF_8));
        return 1;
    }
    public void finish(int root) { }
    public ByteBuffer dataBuffer() { return payload; }
}
