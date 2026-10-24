package com.htc.vr.core;
import android.os.Bundle;
public interface IVRCompositorListener {
    void compositorAvailable(IVRCompositor binder, Bundle info);
    void compositorUnavailable(IVRCompositor binder);
    abstract class Stub implements IVRCompositorListener {}
}
