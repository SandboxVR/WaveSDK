package com.htc.vr.ime.client;

import android.content.ComponentName;
import android.content.Context;
import android.content.ServiceConnection;
import android.os.IBinder;
import com.htc.vr.ime.manager.*;
import com.htc.vr.ime.server.*;

final class IMERetry implements Runnable {
    final IMEManager owner; final long generation;
    IMERetry(IMEManager owner, long generation) { this.owner = owner; this.generation = generation; }
    public void run() { owner.p2Retry(generation); }
}

final class IMEBinding implements ServiceConnection {
    final IMEManager owner; final Context context; final long generation;
    volatile boolean closed;
    boolean bound, unbound, connecting;
    IMEBinding(IMEManager owner, Context context, long generation) { this.owner = owner; this.context = context; this.generation = generation; }
    void accepted(boolean result) { boolean release; synchronized (this) { bound = result; release = closed; } if (release) close(); }
    void close() {
        synchronized (this) { closed = true; if (!bound || unbound) return; unbound = true; }
        try { context.unbindService(this); } catch (RuntimeException failure) { android.util.Log.w("Java_IMEManager", "IME unbind failed", failure); }
    }
    public void onServiceConnected(ComponentName name, IBinder binder) { owner.p2Connected(this, binder); }
    public void onServiceDisconnected(ComponentName name) { owner.p2Disconnected(this); }
    public void onBindingDied(ComponentName name) { owner.p2Disconnected(this); }
    public void onNullBinding(ComponentName name) { owner.p2Disconnected(this); }
}

final class IMECleanup {
    final IMEBinding binding;
    final IIMEManagerAPi1 manager; final IIMEManagerCallbackApi1 managerCallback;
    final IIMEServiceApi1 service; final IIMESCallbackApi1 serviceCallback;
    final IMEDeath managerDeath, serviceDeath;
    IMECleanup(IMEBinding binding, IIMEManagerAPi1 manager, IIMEManagerCallbackApi1 managerCallback,
            IIMEServiceApi1 service, IIMESCallbackApi1 serviceCallback, IMEDeath managerDeath, IMEDeath serviceDeath) {
        this.binding = binding; this.manager = manager; this.managerCallback = managerCallback;
        this.service = service; this.serviceCallback = serviceCallback; this.managerDeath = managerDeath; this.serviceDeath = serviceDeath;
    }
    void close() {
        if (binding != null) binding.close();
        if (managerDeath != null) managerDeath.close();
        if (serviceDeath != null) serviceDeath.close();
        if (manager != null && managerCallback != null) try { manager.delCallback(managerCallback); } catch (android.os.RemoteException | RuntimeException failure) { android.util.Log.w("Java_IMEManager", "Manager callback removal failed", failure); }
        if (service != null && serviceCallback != null) try { service.delCallback(serviceCallback); } catch (android.os.RemoteException | RuntimeException failure) { android.util.Log.w("Java_IMEManager", "IME callback removal failed", failure); }
    }
}

final class IMEDeath implements IBinder.DeathRecipient {
    final IMEManager owner; final long generation; final int type;
    volatile IBinder binder;
    IMEDeath(IMEManager owner, long generation, IBinder binder, int type) { this.owner = owner; this.generation = generation; this.binder = binder; this.type = type; }
    void close() { IBinder value; synchronized (this) { value = binder; binder = null; } if (value != null) try { value.unlinkToDeath(this, 0); } catch (RuntimeException failure) { android.util.Log.w("Java_IMEManager", "IME death unlink failed", failure); } }
    public void binderDied() { close(); owner.p2Died(this); }
}

final class IMEInput extends IIMESCallbackApi1.Stub {
    final IMEManager owner; final long generation, serviceGeneration;
    IMEInput(IMEManager owner, long generation, long serviceGeneration) { this.owner = owner; this.generation = generation; this.serviceGeneration = serviceGeneration; }
    public void onInputSubmit(int id, String content) { owner.p2Input(generation, serviceGeneration, 0, id, content, 0); }
    public void onServiceError(int error) { owner.p2Input(generation, serviceGeneration, 1, 0, null, error); }
    public void onFinishInput() { owner.p2Input(generation, serviceGeneration, 2, 0, null, 0); }
    public void onInputClicked(int id, String content, int code) { owner.p2Input(generation, serviceGeneration, 3, id, content, code); }
}

final class IMEManagerEvents extends IIMEManagerCallbackApi1.Stub {
    final IMEManager owner; final IMEBinding binding;
    IMEManagerEvents(IMEManager owner, IMEBinding binding) { this.owner = owner; this.binding = binding; }
    public void onSwitchIME(IMEServiceItem item) { owner.p2Switch(binding); }
    public void onConnectedIME(IMEServiceItem item) { }
    public void onDisconnectedIME(IMEServiceItem item) { }
    public void onAddedIME(IMEServiceItem[] items) { }
    public void onReplacedIME(IMEServiceItem[] oldItems, IMEServiceItem[] newItems) { }
    public void onRemovedIME(IMEServiceItem[] items) { }
    public void onError(int code, String error) { }
}
