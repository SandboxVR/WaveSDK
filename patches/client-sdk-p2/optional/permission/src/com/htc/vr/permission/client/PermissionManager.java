package com.htc.vr.permission.client;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.*;
import android.content.pm.*;
import android.hardware.usb.*;
import android.os.*;
import android.util.Log;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicBoolean;
import vive.wave.vr.permissionsystem.permission.*;
import vive.wave.vr.oem.lib.IVRConfigService;
import vive.wave.vr.oem.notification.server.*;
import org.json.JSONObject;

/** Selective lifecycle donors. No Binder or application callback runs under the owner monitor. */
public class PermissionManager {
    private static String mPackageName;
    private int connectState, mRetryCount;
    private String[] mRequestString;
    private boolean[] mResultArray;
    private IPermission PermissionService;
    private IUsbPermission UsbPermissionService;
    private boolean isPermissionServiceExist, isInProcess, isOnPause;
    private PermissionCallback mCallback;
    private UsbPermissionCallback mUsbCallback;
    private Thread mWorkThread, mInitThread;
    private PackageManager mPackageManager;
    private Activity mActivity;
    private IVRNServiceAgent mNSAgent;
    private IVRNServiceApi1 mNSApi1;
    private IVRConfigService mConfigs;
    private ServiceConnection mNotificationConnection, mConfigServiceConnection, checkConnection, mConnection, mUsbConnection;
    private boolean isNotificationServiceExist, isInstallPackage, isShow2DNotify, isOEMSerivcesExist, mIsOEMServiceReay;
    private AtomicBoolean mIsRequestUsbPermission, mConfigsReady;
    private UsbDevice mDevice;
    private boolean mResult;
    private IPermissionCallback cb;
    private IUsbPermissionCallback mIUsbCallback;
    private Handler mHandler;
    private Runnable delayseconds;
    private BroadcastReceiver mUsbReceiver;
    // Added fields have JVM default initialization, preserving the shipped constructor.
    private boolean mP2Active;
    private long mP2Generation, mP2Request, mP2UsbSubscription;
    private Context mP2Context;
    private P2UsbReceiver mP2Receiver;
    private ArrayList<P2PermissionBinding> mP2Bindings;
    private Runnable mP2ConfigPoll, mP2NotifyWork;
    private P2UsbReceiver mP2Usb2DReceiver;
    private PendingIntent mP2Usb2DIntent;
    private String mP2Usb2DAction;
    private UsbDevice mP2Usb2DDevice;

    // Compile declarations only; these bodies are excluded from the manifest.
    private static Intent createExplicitFromImplicitIntent(Context context, Intent intent) { return null; }
    public boolean isShow2D() { return false; }

    public void setActivity(Activity activity) {
        synchronized (PermissionManager.class) { if (mP2Active && mActivity == activity) return; }
        release();
        if (activity == null) return;
        Context context = activity.getApplicationContext();
        PackageManager packages = activity.getPackageManager();
        String packageName = activity.getPackageName();
        boolean installed = false;
        for (ApplicationInfo info : packages.getInstalledApplications(128))
            if ("vive.wave.vr.permissionsystem".equals(info.packageName)) { installed = true; break; }
        P2UsbReceiver receiver;
        long generation;
        synchronized (PermissionManager.class) {
            mActivity = activity; mPackageManager = packages; mPackageName = packageName; mP2Context = context;
            mP2Active = true; generation = ++mP2Generation;
            isOnPause = false; isInstallPackage = installed; isShow2DNotify = !installed;
            mConfigsReady.set(false); mIsOEMServiceReay = false;
            mP2Bindings = new ArrayList<P2PermissionBinding>();
            delayseconds = new P2UsbRetry(this, generation);
            receiver = context == null ? null : new P2UsbReceiver(this, context, generation);
            mP2Receiver = receiver;
        }
        if (receiver != null) {
            IntentFilter filter = new IntentFilter(UsbManager.ACTION_USB_DEVICE_ATTACHED);
            filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
            context.registerReceiver(receiver, filter);
            receiver.accepted();
        }
        synchronized (PermissionManager.class) { if (!p2Current(generation)) return; }
        p2ConnectConfig(generation);
    }

    public void release() {
        ArrayList<P2PermissionBinding> bindings;
        P2UsbReceiver receiver;
        Runnable retry, config, notify;
        Thread worker, initializer;
        P2UsbReceiver usbReceiver;
        PendingIntent usbIntent;
        synchronized (PermissionManager.class) {
            mP2Active = false; ++mP2Generation; ++mP2Request;
            retry = delayseconds; config = mP2ConfigPoll; notify = mP2NotifyWork;
            worker = mWorkThread; initializer = mInitThread;
            receiver = mP2Receiver; bindings = mP2Bindings;
            usbReceiver = mP2Usb2DReceiver; usbIntent = mP2Usb2DIntent;
            mP2Usb2DReceiver = null; mP2Usb2DIntent = null; mP2Usb2DAction = null; mP2Usb2DDevice = null;
            mP2Receiver = null; mP2Bindings = null;
            mWorkThread = null; mInitThread = null;
            mActivity = null; mPackageManager = null; mP2Context = null;
            mCallback = null; mUsbCallback = null; mRequestString = null; mResultArray = null; mDevice = null;
            PermissionService = null; UsbPermissionService = null; mConfigs = null; mNSAgent = null; mNSApi1 = null;
            mNotificationConnection = null; mConfigServiceConnection = null; checkConnection = null; mConnection = null; mUsbConnection = null;
            mP2ConfigPoll = null; mP2NotifyWork = null;
            isInProcess = false; isOnPause = true; connectState = 0; mRetryCount = 0;
            isPermissionServiceExist = false; isNotificationServiceExist = false; isOEMSerivcesExist = false;
            mIsOEMServiceReay = false; mConfigsReady.set(false); mIsRequestUsbPermission.set(false);
        }
        if (retry != null) mHandler.removeCallbacks(retry);
        if (config != null) mHandler.removeCallbacks(config);
        if (notify != null) mHandler.removeCallbacks(notify);
        if (worker != null) worker.interrupt();
        if (initializer != null) initializer.interrupt();
        if (receiver != null) receiver.close();
        if (usbReceiver != null) usbReceiver.close();
        if (usbIntent != null) usbIntent.cancel();
        if (bindings != null) for (P2PermissionBinding binding : bindings) binding.close();
    }

    boolean p2Current(long generation) { return mP2Active && mActivity != null && mP2Generation == generation; }
    private boolean p2RequestCurrent(long generation, long request) { return p2Current(generation) && mP2Request == request && isInProcess; }

    private P2PermissionBinding p2Bind(Intent intent, int kind, long generation, long request) {
        P2PermissionBinding binding;
        synchronized (PermissionManager.class) {
            if (!p2Current(generation) || mP2Context == null || intent == null || (request != 0 && !p2RequestCurrent(generation, request))) return null;
            binding = new P2PermissionBinding(this, mP2Context, generation, request, kind);
            mP2Bindings.add(binding);
            if (kind == 0) checkConnection = binding;
            else if (kind == 1) mConnection = binding;
            else if (kind == 2) mUsbConnection = binding;
            else if (kind == 3) mNotificationConnection = binding;
            else mConfigServiceConnection = binding;
        }
        boolean accepted = false;
        try { accepted = binding.context.bindService(intent, binding, Context.BIND_AUTO_CREATE); }
        catch (SecurityException failure) { Log.w("Java_PermissionManager", "Service bind rejected", failure); }
        binding.accepted(accepted);
        boolean stale;
        synchronized (PermissionManager.class) {
            stale = !p2Current(generation);
            if ((!accepted || stale) && mP2Bindings != null) mP2Bindings.remove(binding);
        }
        if (stale) binding.close();
        return accepted && !stale ? binding : null;
    }

    private void p2Close(ServiceConnection connection) {
        if (!(connection instanceof P2PermissionBinding)) return;
        P2PermissionBinding binding = (P2PermissionBinding) connection;
        synchronized (PermissionManager.class) { if (mP2Bindings != null) mP2Bindings.remove(binding); }
        binding.close();
    }

    private void checkServerConnection() {
        ServiceConnection previous; long generation;
        synchronized (PermissionManager.class) { if (!mP2Active) return; previous = checkConnection; checkConnection = null; generation = mP2Generation; }
        p2Close(previous);
        P2PermissionBinding binding = p2Bind(new Intent("vive.wave.vr.permissionsystem.permission.PermissionService").setPackage("vive.wave.vr.permissionsystem"), 0, generation, 0);
        synchronized (PermissionManager.class) { if (p2Current(generation)) isPermissionServiceExist = binding != null; }
    }

    private void connectPermissionService(int type) {
        long generation, request;
        synchronized (PermissionManager.class) { if (!mP2Active || !isInProcess) return; generation = mP2Generation; request = mP2Request; }
        p2ConnectRequest(type, generation, request);
    }

    private void p2ConnectRequest(int type, long generation, long request) {
        synchronized (PermissionManager.class) {
            if (!p2RequestCurrent(generation, request)) return;
            connectState = 1;
            if (type == 1) cb = new P2PermissionResult(this, generation, request);
            else mIUsbCallback = new P2UsbResult(this, generation, request);
        }
        String action = type == 1 ? "vive.wave.vr.permissionsystem.permission.PermissionService" : "vive.wave.vr.permissionsystem.permission.UsbPermissionService";
        P2PermissionBinding binding = p2Bind(new Intent(action).setPackage("vive.wave.vr.permissionsystem"), type, generation, request);
        boolean fail;
        synchronized (PermissionManager.class) {
            fail = p2RequestCurrent(generation, request) && binding == null;
            if (p2Current(generation)) isPermissionServiceExist = binding != null;
            if (fail) { connectState = 0; if (type == 1) mResultArray = new boolean[mRequestString.length]; else { mResult = false; mDevice = null; } }
        }
        if (fail) { if (type == 1) p2CompletePermission(generation, request); else p2CompleteUsb(generation, request); }
    }

    private void connectConfigService() {
        long generation;
        synchronized (PermissionManager.class) { generation = mP2Generation; }
        p2ConnectConfig(generation);
    }

    private void p2ConnectConfig(long generation) {
        Context context;
        synchronized (PermissionManager.class) { if (!p2Current(generation)) return; context = mP2Context; }
        Intent intent = context == null ? null : createExplicitFromImplicitIntent(context, new Intent("com.htc.vr.intent.action.OEMVRConfigService"));
        P2PermissionBinding binding = p2Bind(intent, 4, generation, 0);
        synchronized (PermissionManager.class) {
            if (!p2Current(generation)) return;
            isOEMSerivcesExist = binding != null;
            if (binding == null) { isShow2DNotify = !isInstallPackage; mConfigsReady.set(true); }
        }
    }

    private void connectNotificationService() {
        long generation, request;
        synchronized (PermissionManager.class) { generation = mP2Generation; request = mP2Request; }
        p2ConnectNotification(generation, request);
    }

    private void p2ConnectNotification(long generation, long request) {
        Context context;
        synchronized (PermissionManager.class) { if (!p2RequestCurrent(generation, request)) return; context = mP2Context; }
        Intent intent = context == null ? null : createExplicitFromImplicitIntent(context, new Intent("com.htc.vr.singleton.notification.service"));
        P2PermissionBinding binding = p2Bind(intent, 3, generation, request);
        synchronized (PermissionManager.class) { if (p2RequestCurrent(generation, request)) isNotificationServiceExist = binding != null; }
    }

    void p2Connected(P2PermissionBinding binding, IBinder binder) {
        synchronized (PermissionManager.class) {
            if (!p2Current(binding.generation) || binding.closed || binding.delivered || (binding.request != 0 && !p2RequestCurrent(binding.generation, binding.request))) return;
            binding.delivered = true;
        }
        if (binding.kind == 0) { p2Close(binding); return; }
        if (binding.kind == 1 || binding.kind == 2) {
            IPermission permission = binding.kind == 1 ? IPermission.Stub.asInterface(binder) : null;
            IUsbPermission usb = binding.kind == 2 ? IUsbPermission.Stub.asInterface(binder) : null;
            synchronized (PermissionManager.class) {
                if (!p2RequestCurrent(binding.generation, binding.request) || binding.closed) return;
                if (binding.kind == 1) PermissionService = permission;
                else UsbPermissionService = usb;
                connectState = 2;
            }
            if (binding.kind == 1) p2SendPermission(binding.generation, binding.request); else p2SendUsb(binding.generation, binding.request);
        } else if (binding.kind == 3) {
            Activity activity;
            synchronized (PermissionManager.class) { if (!p2RequestCurrent(binding.generation, binding.request)) return; activity = mActivity; }
            IVRNServiceAgent agent = IVRNServiceAgent.Stub.asInterface(binder);
            IVRNServiceApi1 api = null;
            try { if (agent != null) api = IVRNServiceApi1.Stub.asInterface(agent.getApi(activity.getPackageName() + "/" + activity.getClass().getName(), 1)); }
            catch (RemoteException failure) { Log.w("Java_PermissionManager", "Notification initialization failed", failure); }
            Thread worker;
            synchronized (PermissionManager.class) {
                if (!p2RequestCurrent(binding.generation, binding.request) || binding.closed) return;
                mNSAgent = agent; mNSApi1 = api; worker = mWorkThread;
            }
            if (worker != null && worker.getState() == Thread.State.NEW) worker.start();
        } else {
            IVRConfigService config = IVRConfigService.Stub.asInterface(binder);
            Thread initializer;
            synchronized (PermissionManager.class) {
                if (!p2Current(binding.generation) || binding.closed) return;
                mConfigs = config;
                mP2ConfigPoll = new P2ConfigPoll(this, binding.generation, binding);
                initializer = new Thread(mP2ConfigPoll, "Permission_Client"); mInitThread = initializer;
            }
            initializer.start();
        }
    }

    void p2Disconnected(P2PermissionBinding binding) {
        int complete = 0; Thread worker = null;
        synchronized (PermissionManager.class) {
            if (!p2Current(binding.generation) || binding.closed) return;
            if (binding.kind == 1 && p2RequestCurrent(binding.generation, binding.request)) { mResultArray = new boolean[mRequestString.length]; complete = 1; }
            else if (binding.kind == 2 && p2RequestCurrent(binding.generation, binding.request)) { mResult = false; complete = 2; }
            else if (binding.kind == 4 && mConfigServiceConnection == binding) { mConfigs = null; isShow2DNotify = !isInstallPackage; mConfigsReady.set(true); }
            else if (binding.kind == 3 && mNotificationConnection == binding) { mNSAgent = null; mNSApi1 = null; worker = mWorkThread; }
        }
        p2Close(binding);
        if (complete == 1) p2CompletePermission(binding.generation, binding.request); else if (complete == 2) p2CompleteUsb(binding.generation, binding.request);
        if (worker != null && worker.getState() == Thread.State.NEW) worker.start();
    }

    boolean p2PollConfig(P2ConfigPoll poll) {
        IVRConfigService config; boolean installed;
        synchronized (PermissionManager.class) {
            if (!p2Current(poll.generation) || mP2ConfigPoll != poll || poll.binding.closed) return false;
            config = mConfigs; installed = isInstallPackage;
        }
        boolean show2D = !installed;
        boolean ready = true;
        try {
            String database = config == null ? null : config.getConfigValue("database_ready");
            if (database != null && !Boolean.parseBoolean(database) && poll.attempts++ < 10) ready = false;
            else if (database == null || Boolean.parseBoolean(database)) {
                String json = config == null ? null : config.getConfigValue("PermissionMgr_property");
                if (json != null) {
                    JSONObject root = new JSONObject(json);
                    if (root.has("behavior")) { JSONObject behavior = root.getJSONObject("behavior"); if (behavior.has("show_2d_notify")) show2D = behavior.getBoolean("show_2d_notify"); }
                }
            }
        } catch (Exception failure) { Log.w("Java_PermissionManager", "Configuration unavailable", failure); }
        synchronized (PermissionManager.class) {
            if (!p2Current(poll.generation) || mP2ConfigPoll != poll || poll.binding.closed) return false;
            if (!ready) return true;
            isShow2DNotify = show2D; mIsOEMServiceReay = true; mConfigsReady.set(true); mConfigs = null; mP2ConfigPoll = null;
        }
        p2Close(poll.binding);
        return false;
    }

    public void requestPermissions(String[] names, PermissionCallback callback) {
        long generation;
        synchronized (PermissionManager.class) { if (!mP2Active || names == null) return; generation = mP2Generation; }
        if (isShow2D()) p2RequestPermission2D(names, callback, generation); else p2RequestPermission3D(names, callback, generation);
    }

    private void requestPermissions3D(String[] names, PermissionCallback callback) {
        long generation;
        synchronized (PermissionManager.class) { generation = mP2Generation; }
        p2RequestPermission3D(names, callback, generation);
    }

    private void p2RequestPermission3D(String[] names, PermissionCallback callback, long generation) {
        long request;
        synchronized (PermissionManager.class) {
            if (!p2Current(generation) || !isPermissionServiceExist || isOnPause || isInProcess || names == null) return;
            request = ++mP2Request;
            isInProcess = true; mRequestString = names.clone(); mCallback = callback;
        }
        p2ConnectRequest(1, generation, request);
    }

    private void requestPermission() {
        long generation, request;
        synchronized (PermissionManager.class) { generation = mP2Generation; request = mP2Request; }
        p2SendPermission(generation, request);
    }

    private void p2SendPermission(long generation, long request) {
        IPermission service; IPermissionCallback callback; String[] names; String packageName;
        synchronized (PermissionManager.class) {
            if (!p2RequestCurrent(generation, request) || PermissionService == null || connectState != 2) return;
            service = PermissionService; callback = cb; names = mRequestString; packageName = mPackageName;
        }
        try { service.requestPermissionsWithPackage(packageName, names, callback); }
        catch (RemoteException failure) { p2PermissionResult(generation, request, names, new int[names.length], true); }
    }

    void p2PermissionResult(long generation, long request, String[] names, int[] results, boolean failed) {
        synchronized (PermissionManager.class) {
            if (!p2RequestCurrent(generation, request) || mRequestString == null) return;
            if (names == null || results == null || names.length != results.length || names.length != mRequestString.length) return;
            boolean[] granted = new boolean[mRequestString.length];
            if (!failed) for (int i = 0; i < mRequestString.length; i++) {
                int match = -1;
                for (int j = 0; j < names.length; j++) if (mRequestString[i] != null && mRequestString[i].equals(names[j])) { if (match != -1) return; match = j; }
                if (match == -1) return;
                granted[i] = results[match] == PackageManager.PERMISSION_GRANTED;
            }
            mResultArray = granted;
        }
        p2CompletePermission(generation, request);
    }

    private void requestPermissionCompleted() {
        long generation, request;
        synchronized (PermissionManager.class) { generation = mP2Generation; request = mP2Request; }
        p2CompletePermission(generation, request);
    }

    private void p2CompletePermission(long generation, long request) {
        PermissionCallback callback; GrantedResult result; ServiceConnection binding;
        synchronized (PermissionManager.class) {
            if (!p2RequestCurrent(generation, request)) return;
            callback = mCallback; result = new GrantedResult(mRequestString, mResultArray); binding = mConnection;
            mCallback = null; mRequestString = null; mResultArray = null; mConnection = null;
            PermissionService = null; isInProcess = false; connectState = 0; ++mP2Request;
        }
        p2Close(binding);
        if (callback != null) callback.onRequestCompletedwithObject(result);
    }

    private void requestPermissions2D(String[] names, PermissionCallback callback) {
        long generation;
        synchronized (PermissionManager.class) { generation = mP2Generation; }
        p2RequestPermission2D(names, callback, generation);
    }

    private void p2RequestPermission2D(String[] names, PermissionCallback callback, long generation) {
        long request;
        synchronized (PermissionManager.class) {
            if (!p2Current(generation) || isOnPause || isInProcess) return;
            isInProcess = true; request = ++mP2Request;
            mP2NotifyWork = new P2NotifyWork(this, generation, request, names.clone());
            mWorkThread = new Thread(mP2NotifyWork, "Permission_Client");
        }
        p2ConnectNotification(generation, request);
        boolean fallback;
        synchronized (PermissionManager.class) { fallback = p2RequestCurrent(generation, request) && !isNotificationServiceExist; }
        if (fallback) p2Request2D(generation, request, names);
    }

    private void requestPermissions2D(String[] names) {
        long generation, request;
        synchronized (PermissionManager.class) { generation = mP2Generation; request = mP2Request; }
        p2Request2D(generation, request, names);
    }

    private void p2Request2D(long generation, long request, String[] names) {
        Activity activity; ServiceConnection binding;
        synchronized (PermissionManager.class) {
            if (!p2RequestCurrent(generation, request)) return;
            activity = mActivity; binding = mNotificationConnection;
            isInProcess = false; mNotificationConnection = null; mP2NotifyWork = null; mNSAgent = null; mNSApi1 = null;
        }
        p2Close(binding);
        activity.requestPermissions(names, 1);
    }

    boolean p2Notify(P2NotifyWork work, boolean afterDelay) {
        IVRNServiceApi1 api; Activity activity;
        synchronized (PermissionManager.class) {
            if (!p2RequestCurrent(work.generation, work.request) || mP2NotifyWork != work) return false;
            api = mNSApi1; activity = mActivity;
        }
        if (!afterDelay && api != null) {
            try {
                api.setWhiteName(activity.getPackageName(), true);
                NotificationItem item = new NotificationItem(activity.getResources().getString(R.string.notify_key), activity.getResources().getString(R.string.notify_title), activity.getResources().getString(R.string.notify_content), 1, activity.getPackageName(), 0);
                api.postNotification(item);
                synchronized (PermissionManager.class) { return p2RequestCurrent(work.generation, work.request); }
            } catch (RemoteException failure) { Log.w("Java_PermissionManager", "Notification failed", failure); }
        }
        p2Request2D(work.generation, work.request, work.names);
        return false;
    }

    public void requestUsbPermission(UsbPermissionCallback callback) {
        P2UsbReceiver previousReceiver;
        PendingIntent previousIntent;
        synchronized (PermissionManager.class) {
            if (!mP2Active || isOnPause) return;
            previousReceiver = mP2Usb2DReceiver; previousIntent = mP2Usb2DIntent;
            mP2Usb2DReceiver = null; mP2Usb2DIntent = null; mP2Usb2DAction = null; mP2Usb2DDevice = null;
            mUsbCallback = callback; ++mP2UsbSubscription; mRetryCount = 0;
            if (!isInProcess) mIsRequestUsbPermission.set(false);
            mHandler.removeCallbacks(delayseconds); mHandler.post(delayseconds);
        }
        if (previousReceiver != null) previousReceiver.close();
        if (previousIntent != null) previousIntent.cancel();
    }

    void p2RetryUsb(long generation) {
        synchronized (PermissionManager.class) {
            if (!p2Current(generation) || isOnPause) return;
            if (!mConfigsReady.get() && mRetryCount++ < 10) { mHandler.postDelayed(delayseconds, 200L); return; }
            mRetryCount = 0;
        }
        p2RequestUsb(generation);
    }

    private void requestUsbPermission() {
        long generation;
        synchronized (PermissionManager.class) { generation = mP2Generation; }
        p2RequestUsb(generation);
    }

    private void p2RequestUsb(long generation) {
        Activity activity; long subscription; UsbPermissionCallback callback;
        synchronized (PermissionManager.class) {
            if (!p2Current(generation) || isOnPause || isInProcess || mUsbCallback == null || !mIsRequestUsbPermission.compareAndSet(false, true)) return;
            activity = mActivity; subscription = mP2UsbSubscription; callback = mUsbCallback;
        }
        UsbManager manager = (UsbManager) activity.getSystemService(Context.USB_SERVICE);
        UsbDevice device = null;
        if (manager != null) for (UsbDevice candidate : manager.getDeviceList().values()) { device = candidate; break; }
        if (device == null) { synchronized (PermissionManager.class) { if (p2Current(generation) && mP2UsbSubscription == subscription) mIsRequestUsbPermission.set(false); } return; }
        if (manager.hasPermission(device)) {
            synchronized (PermissionManager.class) { if (!p2Current(generation) || mP2UsbSubscription != subscription) return; mIsRequestUsbPermission.set(false); }
            callback.onRequestCompletedwithObject(new UsbGrantedResult(device, true));
        } else if (isShow2D()) p2RequestUsb2D(device, callback, generation, subscription);
        else p2RequestUsb3D(device, callback, generation, subscription);
    }

    private void requestUsbPermissions2D(UsbDevice device, UsbPermissionCallback callback) {
        long generation, subscription;
        synchronized (PermissionManager.class) { generation = mP2Generation; subscription = mP2UsbSubscription; }
        p2RequestUsb2D(device, callback, generation, subscription);
    }

    private void p2RequestUsb2D(UsbDevice device, UsbPermissionCallback callback, long generation, long subscription) {
        Context context;
        P2UsbReceiver previousReceiver, receiver;
        PendingIntent previousIntent;
        String action = "com.htc.vr.intent.action.usb.permission." + java.util.UUID.randomUUID().toString();
        synchronized (PermissionManager.class) {
            if (!p2Current(generation) || mP2UsbSubscription != subscription || isOnPause || isInProcess) return;
            context = mP2Context; mUsbCallback = callback;
            if (context == null) { mIsRequestUsbPermission.set(false); return; }
            previousReceiver = mP2Usb2DReceiver; previousIntent = mP2Usb2DIntent;
            receiver = new P2UsbReceiver(this, context, generation);
            mP2Usb2DReceiver = receiver; mP2Usb2DIntent = null; mP2Usb2DAction = action; mP2Usb2DDevice = device;
        }
        if (previousReceiver != null) previousReceiver.close();
        if (previousIntent != null) previousIntent.cancel();
        PendingIntent intent = null;
        try {
            context.registerReceiver(receiver, new IntentFilter(action));
            receiver.accepted();
            intent = PendingIntent.getBroadcast(context, 0, new Intent(action).setPackage(context.getPackageName()), 0);
            boolean current;
            synchronized (PermissionManager.class) {
                current = p2Current(generation) && mP2UsbSubscription == subscription && mP2Usb2DReceiver == receiver;
                if (current) mP2Usb2DIntent = intent;
            }
            if (!current) { receiver.close(); intent.cancel(); return; }
            UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
            manager.requestPermission(device, intent);
            synchronized (PermissionManager.class) { if (p2Current(generation) && mP2Usb2DReceiver == receiver) mIsRequestUsbPermission.set(false); }
        } catch (RuntimeException failure) {
            synchronized (PermissionManager.class) {
                if (mP2Usb2DReceiver == receiver) {
                    mP2Usb2DReceiver = null; mP2Usb2DIntent = null; mP2Usb2DAction = null; mP2Usb2DDevice = null;
                    mIsRequestUsbPermission.set(false);
                }
            }
            receiver.accepted(); receiver.close();
            if (intent != null) intent.cancel();
            Log.w("Java_PermissionManager", "USB request setup failed", failure);
        }
    }

    private void requestUsbPermissions3D(UsbDevice device, UsbPermissionCallback callback) {
        long generation, subscription;
        synchronized (PermissionManager.class) { generation = mP2Generation; subscription = mP2UsbSubscription; }
        p2RequestUsb3D(device, callback, generation, subscription);
    }

    private void p2RequestUsb3D(UsbDevice device, UsbPermissionCallback callback, long generation, long subscription) {
        long request;
        synchronized (PermissionManager.class) {
            if (!p2Current(generation) || mP2UsbSubscription != subscription) return;
            if (!isPermissionServiceExist || isOnPause || isInProcess) { mIsRequestUsbPermission.set(false); return; }
            request = ++mP2Request; isInProcess = true; mDevice = device; mUsbCallback = callback;
        }
        p2ConnectRequest(2, generation, request);
    }

    private void requestUsbPermissionService() {
        long generation, request;
        synchronized (PermissionManager.class) { generation = mP2Generation; request = mP2Request; }
        p2SendUsb(generation, request);
    }

    private void p2SendUsb(long generation, long request) {
        IUsbPermission service; IUsbPermissionCallback callback; UsbDevice device;
        synchronized (PermissionManager.class) {
            if (!p2RequestCurrent(generation, request) || UsbPermissionService == null || connectState != 2) return;
            service = UsbPermissionService; callback = mIUsbCallback; device = mDevice;
        }
        try { service.requestUsbPermission(device, callback); }
        catch (RemoteException failure) { p2UsbResult(generation, request, device, false); }
    }

    void p2UsbResult(long generation, long request, UsbDevice device, boolean granted) {
        synchronized (PermissionManager.class) {
            if (!p2RequestCurrent(generation, request) || device == null || mDevice == null || !mDevice.equals(device)) return;
            mResult = granted;
        }
        p2CompleteUsb(generation, request);
    }

    private void requestUsbPermissionCompleted() {
        long generation, request;
        synchronized (PermissionManager.class) { generation = mP2Generation; request = mP2Request; }
        p2CompleteUsb(generation, request);
    }

    private void p2CompleteUsb(long generation, long request) {
        UsbPermissionCallback callback; UsbGrantedResult result; ServiceConnection binding;
        synchronized (PermissionManager.class) {
            if (!p2RequestCurrent(generation, request)) return;
            callback = mUsbCallback; result = new UsbGrantedResult(mDevice, mResult); binding = mUsbConnection;
            UsbPermissionService = null; mDevice = null; mUsbConnection = null; isInProcess = false; connectState = 0;
            mIsRequestUsbPermission.set(false); ++mP2Request;
        }
        p2Close(binding);
        if (callback != null) callback.onRequestCompletedwithObject(result);
    }

    public void onPauseAndReturnFail() {
        long generation, request; int complete = 0;
        boolean twoD = isShow2D();
        synchronized (PermissionManager.class) {
            isOnPause = true; generation = mP2Generation; request = mP2Request;
            if (mP2Active && !twoD && isInProcess) {
                if (mIsRequestUsbPermission.get()) { mResult = false; complete = 2; }
                else { mResultArray = new boolean[mRequestString == null ? 0 : mRequestString.length]; complete = 1; }
            }
        }
        if (complete == 1) p2CompletePermission(generation, request); else if (complete == 2) p2CompleteUsb(generation, request);
    }

    void p2Receive(long generation, Intent intent) {
        if (intent == null) return;
        String action = intent.getAction();
        UsbPermissionCallback callback = null; UsbGrantedResult result = null;
        long request = 0;
        P2UsbReceiver usbReceiver = null;
        PendingIntent usbIntent = null;
        synchronized (PermissionManager.class) {
            if (!p2Current(generation)) return;
            if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {
                if (mUsbCallback != null && !isOnPause) { mRetryCount = 0; mHandler.removeCallbacks(delayseconds); mHandler.post(delayseconds); }
            } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                if (isInProcess && mIsRequestUsbPermission.get()) { mResult = false; request = mP2Request; }
            } else if (mP2Usb2DAction != null && mP2Usb2DAction.equals(action)) {
                UsbDevice device = (UsbDevice) intent.getParcelableExtra("device");
                if (mUsbCallback != null && device != null && device.equals(mP2Usb2DDevice)) {
                    mIsRequestUsbPermission.set(false); callback = mUsbCallback; result = new UsbGrantedResult(device, intent.getBooleanExtra("permission", false));
                    usbReceiver = mP2Usb2DReceiver; usbIntent = mP2Usb2DIntent;
                    mP2Usb2DReceiver = null; mP2Usb2DIntent = null; mP2Usb2DAction = null; mP2Usb2DDevice = null;
                }
            }
        }
        if (request != 0) p2CompleteUsb(generation, request);
        if (usbReceiver != null) usbReceiver.close();
        if (usbIntent != null) usbIntent.cancel();
        if (callback != null) callback.onRequestCompletedwithObject(result);
    }
}
