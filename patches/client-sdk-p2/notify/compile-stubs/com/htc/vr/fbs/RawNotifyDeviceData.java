package com.htc.vr.fbs;
import com.google.flatbuffers.FlatBufferBuilder;
public final class RawNotifyDeviceData {
    public static void startRawNotifyDeviceData(FlatBufferBuilder builder) { }
    public static void addTimestamp(FlatBufferBuilder builder, long timestamp) { }
    public static void addDataStr(FlatBufferBuilder builder, int data) { }
    public static int endRawNotifyDeviceData(FlatBufferBuilder builder) { return 1; }
}
