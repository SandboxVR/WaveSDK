package com.htc.vr.client;
import android.os.IBinder;
import kotlin.Unit;
/** Signature/host model of the unchanged original Kotlin death callback. */
public final class VRNotifyDeviceProvider$mDeathRecipient$1 implements IBinder.DeathRecipient {
    final VRNotifyDeviceProvider this$0;
    VRNotifyDeviceProvider$mDeathRecipient$1(VRNotifyDeviceProvider provider) {
        this$0 = provider;
    }
    @Override public void binderDied() {
        this$0.getManagerClosed().complete(Unit.INSTANCE);
        this$0.close();
    }
}
