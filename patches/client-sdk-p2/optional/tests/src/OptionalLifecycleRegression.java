import android.app.Activity;
import android.content.*;
import android.content.pm.*;
import android.hardware.usb.*;
import android.os.*;
import com.htc.vr.ime.client.*;
import com.htc.vr.ime.manager.*;
import com.htc.vr.ime.server.*;
import com.htc.vr.permission.client.*;
import java.lang.reflect.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import vive.wave.vr.permissionsystem.permission.*;
import vive.wave.vr.oem.lib.IVRConfigService;
import vive.wave.vr.oem.notification.server.*;

/** Loads the actual grafted managers and shipped AIDL classes, with a deterministic Android host. */
public final class OptionalLifecycleRegression {
    static int assertions;
    static void check(boolean value, String message) { assertions++; if (!value) throw new AssertionError(message); }
    static Object field(Object owner, String name) throws Exception { Field f = owner.getClass().getDeclaredField(name); f.setAccessible(true); return f.get(owner); }
    static void field(Object owner, String name, Object value) throws Exception { Field f = owner.getClass().getDeclaredField(name); f.setAccessible(true); f.set(owner, value); }
    static Object staticField(Class<?> owner, String name) throws Exception { Field f = owner.getDeclaredField(name); f.setAccessible(true); return f.get(null); }
    interface Action { void run() throws Exception; }
    static void cross(Action action) throws Exception {
        AtomicReference<Throwable> failure = new AtomicReference<Throwable>();
        Thread thread = new Thread(() -> { try { action.run(); } catch (Throwable caught) { failure.set(caught); } }, "fake-binder-callback");
        thread.setDaemon(true); thread.start(); thread.join(1500L);
        check(!thread.isAlive(), "cross-thread callback must finish without owner-lock deadlock");
        if (failure.get() != null) throw new AssertionError("cross-thread callback failed", failure.get());
    }
    interface RemoteCall { Object call(String name, Object[] arguments) throws Throwable; }
    static <T extends IInterface> T remote(Class<T> type, RemoteCall call) {
        Binder binder = new Binder();
        Object proxy = Proxy.newProxyInstance(type.getClassLoader(), new Class<?>[]{type}, (object, method, args) -> {
            if (method.getName().equals("asBinder")) return binder;
            if (method.getName().equals("toString")) return type.getSimpleName();
            if (method.getName().equals("equals")) return object == args[0];
            if (method.getName().equals("hashCode")) return System.identityHashCode(object);
            Object result = call.call(method.getName(), args == null ? new Object[0] : args);
            if (result != null || !method.getReturnType().isPrimitive()) return result;
            if (method.getReturnType() == boolean.class) return false;
            if (method.getReturnType() == int.class) return 0;
            return null;
        });
        binder.attachInterface((IInterface) proxy, type.getName());
        return type.cast(proxy);
    }
    static final class Host extends Context {
        final PackageManager packages = new PackageManager();
        final List<ServiceConnection> all = new ArrayList<ServiceConnection>();
        final Set<ServiceConnection> bound = Collections.newSetFromMap(new IdentityHashMap<ServiceConnection, Boolean>());
        final Set<BroadcastReceiver> receivers = Collections.newSetFromMap(new IdentityHashMap<BroadcastReceiver, Boolean>());
        final UsbManager usb = new UsbManager();
        int binds, unbinds, registrations, unregistrations;
        boolean accept = true;
        Action duringBind, duringRegister, duringUnbind;
        Host() { packages.services.add(new ResolveInfo()); }
        public Context getApplicationContext() { return this; }
        public boolean bindService(Intent intent, ServiceConnection connection, int flags) {
            binds++; all.add(connection);
            if (accept) bound.add(connection);
            try { if (duringBind != null) duringBind.run(); } catch (Exception failure) { throw new AssertionError(failure); }
            return accept;
        }
        public void unbindService(ServiceConnection connection) {
            try { if (duringUnbind != null) duringUnbind.run(); } catch (Exception failure) { throw new AssertionError(failure); }
            if (!bound.remove(connection)) throw new IllegalArgumentException("not registered");
            unbinds++;
        }
        public PackageManager getPackageManager() { return packages; }
        public String getPackageName() { return "host.app"; }
        public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
            registrations++; receivers.add(receiver);
            try { if (duringRegister != null) duringRegister.run(); } catch (Exception failure) { throw new AssertionError(failure); }
            return null;
        }
        public void unregisterReceiver(BroadcastReceiver receiver) { if (!receivers.remove(receiver)) throw new IllegalArgumentException("not registered"); unregistrations++; }
        public Object getSystemService(String name) { return usb; }
        ServiceConnection last() { return all.get(all.size() - 1); }
        void installed() { ApplicationInfo application = new ApplicationInfo(); application.packageName = "vive.wave.vr.permissionsystem"; packages.applications.add(application); }
    }
    static final class ImeFixture {
        final Host host = new Host();
        final IMEManager manager = new IMEManager();
        final AtomicReference<IIMESCallbackApi1> input = new AtomicReference<IIMESCallbackApi1>();
        final AtomicReference<IIMEManagerCallbackApi1> events = new AtomicReference<IIMEManagerCallbackApi1>();
        int show, noEditor, removedInputs, removedManagers;
        boolean support = true, failShow, failManagerApi, nullManagerApi, failManagerAdd, failServiceAdd;
        Action duringAdd, duringManagerAdd, duringShow, duringRemove;
        final IIMEServiceApi1 service = remote(IIMEServiceApi1.class, (name, args) -> {
            check(!Thread.holdsLock(IMEManager.class), "IME IPC outside owner monitor: " + name);
            if (name.equals("addCallback")) { input.set((IIMESCallbackApi1) args[0]); if (duringAdd != null) duringAdd.run(); if (failServiceAdd) throw new RemoteException("ambiguous service add"); }
            if (name.equals("delCallback")) { removedInputs++; if (duringRemove != null) duringRemove.run(); }
            if (name.equals("isSupportEnableEditorPanel")) return support;
            if (name.equals("showKeyboard") || name.equals("showKeyboardNoEditorPanel")) {
                if (name.equals("showKeyboard")) show++; else noEditor++;
                if (duringShow != null) duringShow.run();
                if (failShow) throw new RemoteException("host show failure");
                return 17;
            }
            return null;
        });
        final IIMEServiceAgent serviceAgent = remote(IIMEServiceAgent.class, (name, args) -> {
            check(!Thread.holdsLock(IMEManager.class), "service-agent IPC outside owner monitor");
            return name.equals("getApi") ? service.asBinder() : null;
        });
        final IIMEManagerAPi1 api = remote(IIMEManagerAPi1.class, (name, args) -> {
            check(!Thread.holdsLock(IMEManager.class), "manager IPC outside owner monitor: " + name);
            if (name.equals("addCallback")) { events.set((IIMEManagerCallbackApi1) args[0]); if (duringManagerAdd != null) duringManagerAdd.run(); if (failManagerAdd) throw new RemoteException("ambiguous manager add"); }
            if (name.equals("delCallback")) removedManagers++;
            return null;
        });
        final IIMEManagerAgent agent = remote(IIMEManagerAgent.class, (name, args) -> {
            check(!Thread.holdsLock(IMEManager.class), "manager-agent IPC outside owner monitor");
            if (name.equals("getApi")) { if (failManagerApi) throw new RemoteException("manager getApi"); return nullManagerApi ? null : api.asBinder(); }
            if (name.equals("getCurrentIMEAgent")) return serviceAgent.asBinder();
            return null;
        });
        Handler handler() throws Exception { return (Handler) field(manager, "handler"); }
        void create() throws Exception { manager.setContext(new Activity(host)); manager.onCreate(); handler().take().run(); }
        void connect() { host.last().onServiceConnected(null, agent.asBinder()); }
    }
    static final class InputCallback implements IMECallback {
        int complete, clicked; InputResult result; Action onComplete;
        public void onInputCompletedwithObject(InputResult value) { check(!Thread.holdsLock(IMEManager.class), "IME application callback outside monitor"); complete++; result = value; try { if (onComplete != null) onComplete.run(); } catch (Exception failure) { throw new AssertionError(failure); } }
        public void onInputClickedwithObject(InputResult value) { check(!Thread.holdsLock(IMEManager.class), "IME click callback outside monitor"); clicked++; result = value; }
    }
    static void testIme() throws Exception {
        ImeFixture pending = new ImeFixture(); pending.manager.setContext(new Activity(pending.host)); pending.manager.onCreate();
        Runnable staleRetry = pending.handler().take();
        pending.manager.onDestroy(); staleRetry.run();
        check(pending.host.binds == 0, "destroy cancels captured initialization retry");
        check(staticField(IMEManager.class, "mContext") == null, "IME clears static context on destroy");
        check(field(pending.manager, "mP2Context") == null, "IME clears instance context on destroy");

        ImeFixture fixture = new ImeFixture(); fixture.create();
        check(fixture.host.binds == 1, "IME binds once while connection pending");
        check(fixture.handler().pending.isEmpty(), "accepted pending bind waits for connection callbacks without idle retry polling");
        if (!fixture.handler().pending.isEmpty()) fixture.handler().take().run();
        check(fixture.host.binds == 1, "retry does not duplicate pending binding");
        fixture.connect();
        check(fixture.manager.isInitialized(), "connected IME initializes");
        check(staticField(IMEManager.class, "mContext") == fixture.host, "IME retains application context");
        InputCallback callback = new InputCallback();
        check(fixture.manager.showKeyboardDefault(new IMEParameter(1), callback, false) == 17 && fixture.noEditor == 1, "no-editor route preserved");
        fixture.support = false;
        check(fixture.manager.showKeyboardDefault(new IMEParameter(1), null, false) == 17 && fixture.show == 1, "legacy keyboard fallback and null callback preserved");
        fixture.failShow = true;
        check(fixture.manager.showKeyboardDefault(new IMEParameter(1), null) == 0, "RemoteException returns original zero code");
        fixture.failShow = false;
        fixture.manager.showKeyboard(new IMEParameter(1), callback);
        fixture.input.get().onInputClicked(9, "key", 13);
        check(callback.clicked == 1 && callback.result.keyCode == 13, "active click result delivered");
        fixture.input.get().onServiceError(8);
        fixture.input.get().onInputSubmit(9, "done");
        check(callback.complete == 1 && callback.result.id == 9 && callback.result.inputContent.equals("done") && callback.result.errorCode == 8, "active completion payload preserved");
        check(field(fixture.manager, "mCallback") == callback && field(fixture.manager, "mParameter") != null, "active no-hide input preserves callback and restore parameter");
        fixture.input.get().onInputSubmit(9, "duplicate");
        check(callback.complete == 2, "repeat submission callback preserved for active no-hide input");
        fixture.manager.restoreShowKeyboard();
        check(fixture.show == 4 && field(fixture.manager, "mCallback") == callback, "restore keeps active callback and keyboard parameter");

        IIMESCallbackApi1 oldInput = fixture.input.get();
        IIMEManagerCallbackApi1 oldEvents = fixture.events.get();
        ServiceConnection oldBinding = fixture.host.last();
        fixture.manager.showKeyboard(new IMEParameter(2), callback);
        fixture.manager.onDestroy();
        check(!fixture.manager.isInitialized() && fixture.host.bound.isEmpty(), "destroy closes active binding");
        check(fixture.removedInputs > 0 && fixture.removedManagers > 0, "destroy unregisters both remote callbacks");
        check(((Binder) fixture.agent.asBinder()).deaths.isEmpty() && ((Binder) fixture.serviceAgent.asBinder()).deaths.isEmpty(), "destroy unlinks both death recipients");
        oldInput.onInputSubmit(1, "late"); oldEvents.onSwitchIME(null); oldBinding.onServiceConnected(null, fixture.agent.asBinder());
        check(callback.complete == 2 && !fixture.manager.isInitialized(), "closed lifecycle ignores stale input and connection callbacks");
        fixture.manager.setContext(new Activity(fixture.host)); fixture.manager.onCreate(); fixture.handler().take().run(); fixture.connect();
        fixture.manager.showKeyboard(new IMEParameter(3), callback);
        oldInput.onInputSubmit(1, "old-generation"); oldEvents.onSwitchIME(null);
        check(callback.complete == 2, "previous lifecycle cannot complete a new input request");
        fixture.input.get().onInputSubmit(3, "new");
        check(callback.complete == 3, "recreated lifecycle still receives current result");
        fixture.manager.release(); fixture.manager.release();
        check(fixture.host.bound.isEmpty(), "release is idempotent");

        ImeFixture bindRace = new ImeFixture();
        bindRace.host.duringBind = () -> cross(() -> bindRace.manager.onDestroy());
        bindRace.create();
        check(bindRace.host.binds == 1 && bindRace.host.unbinds == 1 && bindRace.host.bound.isEmpty(), "destroy during bind return unbinds accepted pending connection exactly once");

        ImeFixture callbackRace = new ImeFixture(); callbackRace.create();
        callbackRace.duringManagerAdd = () -> cross(() -> callbackRace.events.get().onConnectedIME(null));
        callbackRace.duringAdd = () -> cross(() -> callbackRace.input.get().onFinishInput());
        callbackRace.connect();
        InputCallback reentrant = new InputCallback();
        reentrant.onComplete = () -> cross(() -> callbackRace.manager.onDestroy());
        callbackRace.duringShow = () -> cross(() -> callbackRace.input.get().onInputSubmit(5, "sync"));
        callbackRace.manager.showKeyboard(new IMEParameter(5), reentrant);
        check(reentrant.complete == 1 && !callbackRace.manager.isInitialized(), "synchronous cross-thread input and reentrant teardown complete");
        check(field(callbackRace.manager, "mCallback") == null, "show returning after reentrant close cannot restore callback ownership");

        ImeFixture explicit = new ImeFixture(); explicit.create(); explicit.connect(); InputCallback direct = new InputCallback();
        explicit.manager.showKeyboardDefault(new IMEParameter(2), direct); explicit.manager.requestIMECompleted();
        check(direct.complete == 1, "explicit completion after default keyboard route retains original behavior");
        explicit.manager.release();

        for (int mode = 0; mode < 3; mode++) {
            ImeFixture failure = new ImeFixture(); failure.create();
            failure.failManagerApi = mode == 0; failure.nullManagerApi = mode == 1; failure.failManagerAdd = mode == 2;
            failure.connect();
            check(field(failure.manager, "mP2Binding") == null && field(failure.manager, "IIMEManager") == null && field(failure.manager, "mP2ManagerDeath") == null, "failed manager handshake detaches published and pending owner: " + mode);
            check(((Binder) failure.agent.asBinder()).deaths.isEmpty(), "failed pre/post-publication handshake unlinks local death: " + mode);
            check(failure.host.bound.isEmpty() && !failure.handler().pending.isEmpty(), "failed manager handshake cancels bind and schedules retry: " + mode);
            if (mode == 2) check(failure.removedManagers == 1, "ambiguous manager callback registration rolled back");
            failure.failManagerApi = false; failure.nullManagerApi = false; failure.failManagerAdd = false;
            failure.handler().take().run(); failure.connect();
            check(failure.manager.isInitialized(), "manager handshake can recover after failure: " + mode);
            failure.manager.release();
        }

        ImeFixture serviceFailure = new ImeFixture(); serviceFailure.create(); serviceFailure.failServiceAdd = true; serviceFailure.connect();
        IIMESCallbackApi1 invalidInput = serviceFailure.input.get();
        check(!serviceFailure.manager.isInitialized() && serviceFailure.removedInputs == 1 && ((Binder) serviceFailure.serviceAgent.asBinder()).deaths.isEmpty(), "ambiguous service registration rolled back and death unlinked");
        InputCallback guarded = new InputCallback(); serviceFailure.manager.showKeyboard(new IMEParameter(1), guarded);
        invalidInput.onInputSubmit(9, "failed-registration");
        check(guarded.complete == 0, "failed service callback cannot complete an input");
        serviceFailure.failServiceAdd = false; serviceFailure.events.get().onSwitchIME(null);
        check(serviceFailure.manager.isInitialized(), "service registration can recover on switch");
        serviceFailure.input.get().onInputSubmit(2, "valid");
        check(guarded.complete == 1, "recovered service delivers active callback");
        IIMEManagerCallbackApi1 retiredManager = serviceFailure.events.get(); ServiceConnection retiredBinding = serviceFailure.host.last();
        retiredBinding.onServiceDisconnected(null); serviceFailure.handler().take().run(); serviceFailure.connect();
        IIMESCallbackApi1 currentInput = serviceFailure.input.get(); retiredManager.onSwitchIME(null);
        check(serviceFailure.input.get() == currentInput, "retired manager callback cannot switch replacement binding service");
        serviceFailure.manager.hideKeyboard(); serviceFailure.manager.release();

        ImeFixture exhausted = new ImeFixture(); exhausted.failManagerApi = true; exhausted.create(); exhausted.connect();
        int attempts = 0;
        while (!exhausted.handler().pending.isEmpty() && attempts++ < 10) { exhausted.handler().take().run(); exhausted.connect(); }
        check(exhausted.host.binds == 4 && exhausted.handler().pending.isEmpty(), "persistent handshake failure stops after initial attempt and three delayed retries");
        exhausted.manager.release();
        exhausted.manager.setContext(new Activity(exhausted.host)); exhausted.failManagerApi = false; exhausted.manager.onCreate(); exhausted.handler().take().run(); exhausted.connect();
        check(exhausted.manager.isInitialized(), "explicit lifecycle restart resets exhausted retry budget");
        exhausted.manager.release();
    }

    static final class PermissionFixture {
        final Host host = new Host();
        final Activity activity = new Activity(host);
        final PermissionManager manager = new PermissionManager();
        final AtomicReference<IPermissionCallback> callback = new AtomicReference<IPermissionCallback>();
        final AtomicReference<IUsbPermissionCallback> usbCallback = new AtomicReference<IUsbPermissionCallback>();
        int requests, usbRequests;
        boolean fail;
        Action duringRequest;
        final IPermission service = remote(IPermission.class, (name, args) -> {
            check(!Thread.holdsLock(PermissionManager.class), "permission IPC outside owner monitor");
            if (name.equals("requestPermissionsWithPackage")) { requests++; callback.set((IPermissionCallback) args[2]); if (duringRequest != null) duringRequest.run(); if (fail) throw new RemoteException("host failure"); }
            return null;
        });
        final IUsbPermission usb = remote(IUsbPermission.class, (name, args) -> {
            check(!Thread.holdsLock(PermissionManager.class), "USB IPC outside owner monitor");
            if (name.equals("requestUsbPermission")) { usbRequests++; usbCallback.set((IUsbPermissionCallback) args[1]); }
            return null;
        });
        PermissionFixture() { host.installed(); }
        void create() { manager.setActivity(activity); }
        void ready() throws Exception { field(manager, "isPermissionServiceExist", true); field(manager, "mConfigsReady", new AtomicBoolean(true)); }
        Handler handler() throws Exception { return (Handler) field(manager, "mHandler"); }
        void connect() { host.last().onServiceConnected(null, service.asBinder()); }
    }
    static void testPermission() throws Exception {
        PermissionFixture fixture = new PermissionFixture(); fixture.host.packages.services.clear(); fixture.create(); fixture.ready();
        AtomicInteger delivered = new AtomicInteger(); AtomicReference<GrantedResult> result = new AtomicReference<GrantedResult>();
        PermissionCallback application = value -> { check(!Thread.holdsLock(PermissionManager.class), "permission application callback outside monitor"); delivered.incrementAndGet(); result.set(value); };
        fixture.manager.requestPermissions(new String[]{"a", "b"}, application); fixture.connect();
        check(fixture.requests == 1, "normal VR permission request sent");
        IPermissionCallback first = fixture.callback.get();
        first.updatePermissionResult(null, null);
        first.updatePermissionResult(new String[]{"a"}, new int[]{0});
        first.updatePermissionResult(new String[]{"x", "b"}, new int[]{0, 0});
        check(delivered.get() == 0, "malformed or unrelated permission replies ignored");
        first.updatePermissionResult(new String[]{"b", "a"}, new int[]{-1, 0});
        check(delivered.get() == 1 && result.get().result[0] && !result.get().result[1], "result maps original request names despite reordered reply");
        check(field(fixture.manager, "mCallback") == null && field(fixture.manager, "mRequestString") == null, "completed permission releases request owners");
        first.updatePermissionResult(new String[]{"a", "b"}, new int[]{0, 0});
        check(delivered.get() == 1, "duplicate permission result ignored");
        fixture.manager.requestPermissions(new String[]{"a"}, application); fixture.connect();
        IPermissionCallback second = fixture.callback.get();
        first.updatePermissionResult(new String[]{"a", "b"}, new int[]{0, 0});
        check(delivered.get() == 1, "previous request cannot finish current request");
        fixture.manager.requestPermissions(new String[]{"ignored-overlap"}, application);
        second.updatePermissionResult(new String[]{"a"}, new int[]{0});
        check(delivered.get() == 2 && result.get().requestPermissions[0].equals("a"), "overlapping request preserves active request");

        fixture.fail = true;
        fixture.manager.requestPermissions(new String[]{"a"}, application); fixture.connect();
        check(delivered.get() == 3 && !result.get().result[0], "remote failure completes denied result");
        fixture.fail = false;
        fixture.manager.requestPermissions(new String[]{"a"}, null); fixture.connect(); fixture.callback.get().updatePermissionResult(new String[]{"a"}, new int[]{0});
        check(!(Boolean) field(fixture.manager, "isInProcess"), "null application callback still releases request");

        fixture.manager.requestPermissions(new String[]{"a"}, application); fixture.connect();
        IPermissionCallback beforePause = fixture.callback.get(); fixture.manager.onPauseAndReturnFail();
        check(delivered.get() == 4 && !result.get().result[0], "pause returns denied result for active VR request");
        beforePause.updatePermissionResult(new String[]{"a"}, new int[]{0});
        check(delivered.get() == 4, "reply after pause completion ignored");
        fixture.manager.onResume();

        fixture.manager.requestPermissions(new String[]{"a"}, application); fixture.connect();
        IPermissionCallback old = fixture.callback.get(); ServiceConnection oldBinding = fixture.host.last();
        BroadcastReceiver oldReceiver = fixture.host.receivers.iterator().next(); Runnable oldRetry = (Runnable) field(fixture.manager, "delayseconds");
        fixture.manager.release();
        check(field(fixture.manager, "mActivity") == null && field(fixture.manager, "mPackageManager") == null && field(fixture.manager, "mCallback") == null && field(fixture.manager, "mUsbCallback") == null, "release detaches Activity, PackageManager and callbacks");
        check(fixture.host.receivers.isEmpty() && fixture.host.bound.isEmpty(), "release unregisters receiver and closes all pending/active bindings");
        old.updatePermissionResult(new String[]{"a"}, new int[]{0}); oldBinding.onServiceConnected(null, fixture.service.asBinder()); oldRetry.run(); oldReceiver.onReceive(fixture.host, new Intent(UsbManager.ACTION_USB_DEVICE_ATTACHED));
        check(delivered.get() == 4 && fixture.handler().pending.isEmpty(), "stale permission callbacks, retries and receiver cannot restart released lifecycle");
        fixture.create(); fixture.ready(); fixture.manager.requestPermissions(new String[]{"a"}, application); fixture.connect();
        old.updatePermissionResult(new String[]{"a"}, new int[]{0});
        check(delivered.get() == 4, "old lifecycle cannot deliver into replacement Activity");
        fixture.callback.get().updatePermissionResult(new String[]{"a"}, new int[]{0});
        check(delivered.get() == 5, "replacement lifecycle receives current result");
        fixture.manager.release(); fixture.manager.release();
        check(fixture.host.registrations == fixture.host.unregistrations, "idempotent receiver teardown balances registrations");

        PermissionFixture bindRace = new PermissionFixture(); bindRace.host.packages.services.clear(); bindRace.create(); bindRace.ready();
        bindRace.host.duringBind = () -> cross(() -> bindRace.manager.release());
        bindRace.manager.requestPermissions(new String[]{"a"}, application);
        check(bindRace.host.bound.isEmpty() && field(bindRace.manager, "mActivity") == null, "release during bind return cancels accepted binding");

        PermissionFixture registrationRace = new PermissionFixture(); registrationRace.host.packages.services.clear();
        registrationRace.host.duringRegister = () -> cross(() -> registrationRace.manager.release()); registrationRace.create();
        check(registrationRace.host.receivers.isEmpty() && field(registrationRace.manager, "mActivity") == null, "release racing receiver registration unregisters late accepted receiver");

        PermissionFixture sync = new PermissionFixture(); sync.host.packages.services.clear(); sync.create(); sync.ready();
        sync.duringRequest = () -> cross(() -> sync.callback.get().updatePermissionResult(new String[]{"a"}, new int[]{0}));
        sync.manager.requestPermissions(new String[]{"a"}, value -> { check(!Thread.holdsLock(PermissionManager.class), "reentrant permission callback outside monitor"); try { cross(() -> sync.manager.release()); } catch (Exception failure) { throw new AssertionError(failure); } });
        sync.connect();
        check(field(sync.manager, "mActivity") == null && sync.host.bound.isEmpty(), "synchronous cross-thread result and reentrant release finish without deadlock");

        PermissionFixture twoD = new PermissionFixture(); twoD.host.packages.applications.clear(); twoD.host.packages.services.clear(); twoD.create();
        AtomicInteger twoDCallback = new AtomicInteger(); twoD.manager.requestPermissions(new String[]{"camera"}, value -> twoDCallback.incrementAndGet());
        check(twoD.activity.requested == 1 && twoDCallback.get() == 0, "2D fallback preserves Activity.requestPermissions and Unity callback contract");
        twoD.manager.release();

        PermissionFixture configured = new PermissionFixture(); configured.create();
        IVRConfigService config = remote(IVRConfigService.class, (name, args) -> {
            check(!Thread.holdsLock(PermissionManager.class), "config IPC outside owner monitor");
            return name.equals("getConfigValue") && "database_ready".equals(args[0]) ? "true" : null;
        });
        configured.host.last().onServiceConnected(null, config.asBinder());
        Thread initializer = (Thread) field(configured.manager, "mInitThread"); initializer.join(2000L);
        check(!initializer.isAlive() && ((AtomicBoolean) field(configured.manager, "mConfigsReady")).get() && configured.host.bound.isEmpty(), "OEM config initializes in background and releases its binding");
        configured.manager.release();

        PermissionFixture notification = new PermissionFixture(); notification.create(); field(notification.manager, "isShow2DNotify", true);
        AtomicInteger posted = new AtomicInteger();
        IVRNServiceApi1 notificationApi = remote(IVRNServiceApi1.class, (name, args) -> { check(!Thread.holdsLock(PermissionManager.class), "notification IPC outside owner monitor"); if (name.equals("postNotification")) posted.incrementAndGet(); return null; });
        IVRNServiceAgent notificationAgent = remote(IVRNServiceAgent.class, (name, args) -> { check(!Thread.holdsLock(PermissionManager.class), "notification-agent IPC outside owner monitor"); return name.equals("getApi") ? notificationApi.asBinder() : null; });
        notification.manager.requestPermissions(new String[]{"camera"}, value -> { throw new AssertionError("2D Activity route must retain original callback contract"); });
        notification.host.last().onServiceConnected(null, notificationAgent.asBinder());
        Thread work = (Thread) field(notification.manager, "mWorkThread"); work.join(2000L);
        check(!work.isAlive() && posted.get() == 1 && notification.activity.requested == 1, "2D notification path posts then requests Activity permissions after original delay");
        notification.manager.release();

        PermissionFixture usb = new PermissionFixture(); usb.host.packages.services.clear(); usb.create(); usb.ready();
        UsbDevice device = new UsbDevice("one"); usb.host.usb.devices.put("one", device);
        AtomicInteger usbDelivered = new AtomicInteger(); AtomicReference<UsbGrantedResult> usbResult = new AtomicReference<UsbGrantedResult>();
        usb.manager.requestUsbPermission(value -> { check(!Thread.holdsLock(PermissionManager.class), "USB application callback outside monitor"); usbDelivered.incrementAndGet(); usbResult.set(value); });
        usb.handler().take().run(); usb.host.last().onServiceConnected(null, usb.usb.asBinder());
        check(usb.usbRequests == 1, "normal USB service request sent");
        IUsbPermissionCallback staleUsb = usb.usbCallback.get();
        staleUsb.updatePermissionResult(new UsbDevice("wrong"), true);
        check(usbDelivered.get() == 0, "unrelated USB device reply ignored");
        staleUsb.updatePermissionResult(device, true);
        check(usbDelivered.get() == 1 && usbResult.get().device == device && usbResult.get().result, "active USB result delivered");
        staleUsb.updatePermissionResult(device, true);
        check(usbDelivered.get() == 1, "duplicate USB result suppressed");
        usb.manager.release(); staleUsb.updatePermissionResult(device, false);
        check(usbDelivered.get() == 1, "USB result after release ignored");

        PermissionFixture usb2D = new PermissionFixture(); usb2D.host.packages.applications.clear(); usb2D.host.packages.services.clear(); usb2D.create();
        usb2D.host.usb.devices.put("one", device);
        AtomicInteger twoDUsbDelivered = new AtomicInteger();
        usb2D.manager.requestUsbPermission(value -> twoDUsbDelivered.incrementAndGet()); usb2D.handler().take().run();
        android.app.PendingIntent oldIntent = usb2D.host.usb.pending;
        BroadcastReceiver oldUsbReceiver = (BroadcastReceiver) field(usb2D.manager, "mP2Usb2DReceiver");
        String oldAction = oldIntent.intent.getAction();
        check(usb2D.host.receivers.size() == 2, "2D USB pending request owns separate result receiver");
        usb2D.manager.release();

        PermissionFixture usbSetupFailure = new PermissionFixture(); usbSetupFailure.host.packages.applications.clear(); usbSetupFailure.host.packages.services.clear(); usbSetupFailure.create();
        usbSetupFailure.host.usb.devices.put("one", device); usbSetupFailure.host.usb.failRequest = true;
        usbSetupFailure.manager.requestUsbPermission(value -> { }); usbSetupFailure.handler().take().run();
        check(usbSetupFailure.host.receivers.size() == 1 && usbSetupFailure.host.usb.pending.cancelled && !((AtomicBoolean) field(usbSetupFailure.manager, "mIsRequestUsbPermission")).get(), "USB OS setup exception cancels receiver, token, and pending latch");
        usbSetupFailure.manager.release();

        PermissionFixture enumeration = new PermissionFixture(); enumeration.host.packages.applications.clear(); enumeration.host.packages.services.clear(); enumeration.create(); enumeration.host.usb.devices.put("one", device);
        UsbPermissionCallback replacement = value -> { };
        enumeration.host.usb.beforePermissionCheck = () -> { enumeration.manager.release(); enumeration.manager.setActivity(new Activity(enumeration.host)); enumeration.manager.requestUsbPermission(replacement); };
        enumeration.manager.requestUsbPermission(value -> { throw new AssertionError("obsolete callback"); }); enumeration.handler().take().run();
        check(field(enumeration.manager, "mUsbCallback") == replacement && enumeration.host.usb.pending == null, "USB enumeration from retired lifecycle cannot revive old callback or launch OS request");
        enumeration.host.usb.beforePermissionCheck = null; enumeration.handler().take().run();
        check(enumeration.host.usb.pending != null, "replacement USB subscription still starts normally");
        enumeration.manager.release();
        check(oldIntent.cancelled && usb2D.host.receivers.isEmpty(), "release cancels pending OS USB request and both receivers");
        usb2D.create(); usb2D.manager.requestUsbPermission(value -> twoDUsbDelivered.incrementAndGet()); usb2D.handler().take().run();
        android.app.PendingIntent currentIntent = usb2D.host.usb.pending;
        BroadcastReceiver currentUsbReceiver = (BroadcastReceiver) field(usb2D.manager, "mP2Usb2DReceiver");
        check(!oldAction.equals(currentIntent.intent.getAction()), "replacement USB request has unique PendingIntent action identity");
        Intent staleResult = new Intent(oldAction).putExtra("device", device).putExtra("permission", true);
        oldUsbReceiver.onReceive(usb2D.host, staleResult); currentUsbReceiver.onReceive(usb2D.host, staleResult);
        check(twoDUsbDelivered.get() == 0, "old OS result cannot target new lifecycle callback even through new receiver");
        currentUsbReceiver.onReceive(usb2D.host, new Intent(currentIntent.intent.getAction()).putExtra("device", new UsbDevice("wrong")).putExtra("permission", true));
        check(twoDUsbDelivered.get() == 0, "2D USB response requires expected pending device");
        Intent currentResult = new Intent(currentIntent.intent.getAction()).putExtra("device", device).putExtra("permission", true);
        currentUsbReceiver.onReceive(usb2D.host, currentResult); currentUsbReceiver.onReceive(usb2D.host, currentResult);
        check(twoDUsbDelivered.get() == 1 && currentIntent.cancelled && usb2D.host.receivers.size() == 1, "2D USB completion is once, cancels OS token, preserves active hotplug receiver");
        usb2D.manager.release();
    }

    public static void main(String[] args) throws Exception { testIme(); testPermission(); System.out.println("Optional lifecycle regression passed: " + assertions + " assertions (actual grafted JARs)"); }
}
