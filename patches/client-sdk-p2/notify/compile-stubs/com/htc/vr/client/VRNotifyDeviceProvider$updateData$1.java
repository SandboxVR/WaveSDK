package com.htc.vr.client;
import com.google.flatbuffers.FlatBufferBuilder;
import java.nio.ByteBuffer;
import kotlin.Unit;
import kotlin.jvm.functions.Function1;
/** Signature/host model of the unchanged original Kotlin buffer-copy lambda. */
final class VRNotifyDeviceProvider$updateData$1 implements Function1<ByteBuffer, Unit> {
    private final FlatBufferBuilder builder;
    VRNotifyDeviceProvider$updateData$1(FlatBufferBuilder builder) { this.builder = builder; }
    @Override public Unit invoke(ByteBuffer value) {
        value.put(builder.dataBuffer());
        return Unit.INSTANCE;
    }
}
