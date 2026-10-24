package com.htc.vr.permission.client;

import android.content.*;
import android.hardware.usb.UsbDevice;
import android.os.IBinder;
import vive.wave.vr.permissionsystem.permission.*;

final class P2PermissionBinding implements ServiceConnection {
    final PermissionManager owner; final Context context; final long generation, request; final int kind;
    volatile boolean closed;
    boolean bound, unbound, delivered;
    P2PermissionBinding(PermissionManager owner, Context context, long generation, long request, int kind) { this.owner = owner; this.context = context; this.generation = generation; this.request = request; this.kind = kind; }
    void accepted(boolean result) { boolean release; synchronized (this) { bound = result; release = closed; } if (release) close(); }
    void close() { synchronized (this) { closed = true; if (!bound || unbound) return; unbound = true; } try { context.unbindService(this); } catch (RuntimeException failure) { android.util.Log.w("Java_PermissionManager", "Service unbind failed", failure); } }
    public void onServiceConnected(ComponentName name, IBinder binder) { owner.p2Connected(this, binder); }
    public void onServiceDisconnected(ComponentName name) { owner.p2Disconnected(this); }
    public void onBindingDied(ComponentName name) { owner.p2Disconnected(this); }
    public void onNullBinding(ComponentName name) { owner.p2Disconnected(this); }
}

final class P2PermissionResult extends IPermissionCallback.Stub {
    final PermissionManager owner; final long generation, request;
    P2PermissionResult(PermissionManager owner, long generation, long request) { this.owner = owner; this.generation = generation; this.request = request; }
    public void updatePermissionResult(String[] names, int[] results) { owner.p2PermissionResult(generation, request, names, results, false); }
    public void onRequestPermissionsResult(int code, String[] names, int[] results) { }
}

final class P2UsbResult extends IUsbPermissionCallback.Stub {
    final PermissionManager owner; final long generation, request;
    P2UsbResult(PermissionManager owner, long generation, long request) { this.owner = owner; this.generation = generation; this.request = request; }
    public void updatePermissionResult(UsbDevice device, boolean granted) { owner.p2UsbResult(generation, request, device, granted); }
}

final class P2UsbReceiver extends BroadcastReceiver {
    final PermissionManager owner; final Context context; final long generation;
    boolean registered, closed, unregistered;
    P2UsbReceiver(PermissionManager owner, Context context, long generation) { this.owner = owner; this.context = context; this.generation = generation; }
    void accepted() { boolean release; synchronized (this) { registered = true; release = closed; } if (release) close(); }
    void close() { synchronized (this) { closed = true; if (!registered || unregistered) return; unregistered = true; } try { context.unregisterReceiver(this); } catch (RuntimeException failure) { android.util.Log.w("Java_PermissionManager", "Receiver unregister failed", failure); } }
    public void onReceive(Context context, Intent intent) { owner.p2Receive(generation, intent); }
}

final class P2UsbRetry implements Runnable {
    final PermissionManager owner; final long generation;
    P2UsbRetry(PermissionManager owner, long generation) { this.owner = owner; this.generation = generation; }
    public void run() { owner.p2RetryUsb(generation); }
}

final class P2ConfigPoll implements Runnable {
    final PermissionManager owner; final long generation; final P2PermissionBinding binding; int attempts;
    P2ConfigPoll(PermissionManager owner, long generation, P2PermissionBinding binding) { this.owner = owner; this.generation = generation; this.binding = binding; }
    public void run() { try { while (owner.p2PollConfig(this)) Thread.sleep(500L); } catch (InterruptedException cancelled) { Thread.currentThread().interrupt(); } }
}

final class P2NotifyWork implements Runnable {
    final PermissionManager owner; final long generation, request; final String[] names;
    P2NotifyWork(PermissionManager owner, long generation, long request, String[] names) { this.owner = owner; this.generation = generation; this.request = request; this.names = names; }
    public void run() { try { if (owner.p2Notify(this, false)) { Thread.sleep(500L); owner.p2Notify(this, true); } } catch (InterruptedException cancelled) { Thread.currentThread().interrupt(); } }
}
