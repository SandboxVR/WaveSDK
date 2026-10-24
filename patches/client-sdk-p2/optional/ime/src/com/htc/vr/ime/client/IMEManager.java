package com.htc.vr.ime.client;

import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import com.htc.vr.ime.manager.*;
import com.htc.vr.ime.server.*;

/** Method donors only. The shipped constructor and all unlisted methods stay intact. */
public class IMEManager {
    private static Context mContext;
    private static IMEServiceItem mIMEServiceItem;
    private int connectState, managerConnectState, mId, mErrorCode;
    private String mInputContent;
    private IIMEServiceApi1 IMEService;
    private IIMEManagerAPi1 IIMEManager;
    private IIMEManagerAgent mIMEManagerAgaent;
    private boolean isIMEManagerExist, mShowOnVRScene, isInProcess, isOnPause;
    private IMEParameter mParameter;
    private IMECallback mCallback;
    private Runnable requestIMERunnable;
    private Handler handler;
    private IIMESCallbackApi1 imeClientCB;
    private IIMEManagerCallbackApi1 imeManagerClientCB;
    private ServiceConnection managerConnection;
    // Added fields need no constructor changes.
    private boolean mP2Active;
    private int mP2HandshakeFailures;
    private long mP2Generation, mP2ServiceGeneration, mP2InputRequest;
    private Context mP2Context;
    private IMEBinding mP2Binding;
    private IMEDeath mP2ManagerDeath, mP2ServiceDeath;

    public void setContext(Context context) {
        Context application = context == null ? null : context.getApplicationContext();
        if (application == null && context != null && !(context instanceof android.app.Activity)) application = context;
        boolean changed;
        synchronized (IMEManager.class) { changed = mP2Active && mP2Context != application; }
        if (changed) onDestroy();
        synchronized (IMEManager.class) { mP2Context = application; mContext = application; }
    }

    public void onCreate() {
        synchronized (IMEManager.class) {
            if (mP2Active) return;
            mP2Active = true;
            mP2HandshakeFailures = 0;
            long generation = ++mP2Generation;
            isOnPause = false;
            requestIMERunnable = new IMERetry(this, generation);
            handler.postDelayed(requestIMERunnable, 500L);
        }
    }

    public void onDestroy() {
        IMECleanup cleanup;
        Runnable retry;
        synchronized (IMEManager.class) {
            mP2Active = false;
            ++mP2Generation;
            retry = requestIMERunnable;
            cleanup = p2Detach();
            mCallback = null; mParameter = null; mInputContent = null;
            isInProcess = false; isOnPause = true; mShowOnVRScene = false;
            mIMEServiceItem = null;
            if (mContext == mP2Context) mContext = null;
            mP2Context = null;
        }
        if (retry != null) handler.removeCallbacks(retry);
        cleanup.close();
    }

    public void release() { onDestroy(); }
    boolean p2Current(long generation) { return mP2Active && generation == mP2Generation; }

    void p2Retry(long generation) {
        IMEBinding binding;
        boolean servicePending;
        synchronized (IMEManager.class) { if (!p2Current(generation)) return; binding = mP2Binding; servicePending = IIMEManager != null && IMEService == null; }
        if (servicePending) p2ConnectService(binding); else p2ConnectManager(generation);
    }

    private void p2ScheduleRetry(long generation) {
        synchronized (IMEManager.class) {
            if (!p2Current(generation)) return;
            handler.removeCallbacks(requestIMERunnable);
            if (++mP2HandshakeFailures <= 3) handler.postDelayed(requestIMERunnable, 500L);
        }
    }

    private void connectIIMEManager() {
        long generation;
        synchronized (IMEManager.class) { generation = mP2Generation; }
        p2ConnectManager(generation);
    }

    private void p2ConnectManager(long generation) {
        Context context;
        synchronized (IMEManager.class) {
            if (!p2Current(generation) || mP2Context == null || mP2Binding != null) return;
            context = mP2Context;
        }
        Intent intent = IMEServiceSetting.getManagerServiceIntent(context);
        IMEBinding binding;
        synchronized (IMEManager.class) {
            if (!p2Current(generation) || mP2Context != context || mP2Binding != null) return;
            if (intent == null) { isIMEManagerExist = false; return; }
            binding = new IMEBinding(this, context, generation);
            mP2Binding = binding; managerConnection = binding; managerConnectState = 1;
        }
        boolean accepted = false;
        try { accepted = context.bindService(intent, binding, Context.BIND_AUTO_CREATE); }
        catch (SecurityException failure) { Log.e("Java_IMEManager", "IME bind rejected", failure); }
        binding.accepted(accepted);
        boolean stale;
        synchronized (IMEManager.class) {
            stale = mP2Binding != binding || !p2Current(generation);
            if (!stale) {
                isIMEManagerExist = accepted;
                if (!accepted) { mP2Binding = null; managerConnectState = 0; }
            }
        }
        if (stale) binding.close();
    }

    void p2Connected(IMEBinding binding, IBinder binder) {
        synchronized (IMEManager.class) { if (mP2Binding != binding || !p2Current(binding.generation) || binding.closed || binding.connecting || mP2ManagerDeath != null) return; binding.connecting = true; }
        IMEDeath death = new IMEDeath(this, binding.generation, binder, 0);
        IIMEManagerAPi1 manager = null;
        IIMEManagerCallbackApi1 callback = null;
        try {
            IIMEManagerAgent agent = IIMEManagerAgent.Stub.asInterface(binder);
            if (agent == null) { p2ConnectFailed(binding, null, null, death); return; }
            binder.linkToDeath(death, 0);
            manager = IIMEManagerAPi1.Stub.asInterface(agent.getApi("UnityPlugin_IMEManger", 1));
            synchronized (IMEManager.class) {
                if (mP2Binding != binding || !p2Current(binding.generation) || binding.closed || death.binder == null) { /* rollback outside monitor */ }
                else {
                    mP2ManagerDeath = death; mIMEManagerAgaent = agent; IIMEManager = manager;
                    if (manager != null) {
                        managerConnectState = 2; callback = new IMEManagerEvents(this, binding);
                        imeManagerClientCB = callback;
                    }
                }
            }
            if (callback == null) { p2ConnectFailed(binding, manager, callback, death); return; }
            manager.addCallback(callback);
            boolean current;
            synchronized (IMEManager.class) { current = p2Current(binding.generation) && mP2Binding == binding && IIMEManager == manager; }
            if (current) p2ConnectService(binding);
            else new IMECleanup(null, manager, callback, null, null, death, null).close();
        } catch (RemoteException | RuntimeException failure) {
            p2ConnectFailed(binding, manager, callback, death);
            Log.e("Java_IMEManager", "IME connection failed", failure);
        } finally {
            synchronized (IMEManager.class) { binding.connecting = false; }
        }
    }

    private void p2ConnectFailed(IMEBinding binding, IIMEManagerAPi1 manager, IIMEManagerCallbackApi1 callback, IMEDeath death) {
        IMECleanup cleanup;
        boolean retry = false;
        synchronized (IMEManager.class) {
            if (mP2Binding == binding && p2Current(binding.generation)) {
                cleanup = p2Detach();
                retry = true;
            } else cleanup = new IMECleanup(null, manager, callback, null, null, death, null);
        }
        cleanup.close();
        if (cleanup.manager != manager || cleanup.managerCallback != callback)
            new IMECleanup(null, manager, callback, null, null, null, null).close();
        death.close();
        if (retry) p2ScheduleRetry(binding.generation);
    }

    void p2Disconnected(IMEBinding binding) {
        IMECleanup cleanup;
        synchronized (IMEManager.class) {
            if (mP2Binding != binding || !p2Current(binding.generation)) return;
            cleanup = p2Detach();
        }
        cleanup.close();
        p2ScheduleRetry(binding.generation);
    }

    private void connectIMEService() {
        IMEBinding binding;
        synchronized (IMEManager.class) { binding = mP2Binding; }
        p2ConnectService(binding);
    }

    private void p2ConnectService(IMEBinding binding) {
        IIMEManagerAgent agent;
        long generation, serial;
        IMECleanup previous;
        synchronized (IMEManager.class) {
            if (binding == null || mP2Binding != binding || !p2Current(binding.generation) || binding.closed || mIMEManagerAgaent == null) return;
            generation = mP2Generation; agent = mIMEManagerAgaent;
            serial = ++mP2ServiceGeneration;
            previous = new IMECleanup(null, null, null, IMEService, imeClientCB, null, mP2ServiceDeath);
            IMEService = null; mP2ServiceDeath = null; connectState = 0;
        }
        previous.close();
        IMEDeath death = null;
        IIMEServiceApi1 service = null;
        IIMESCallbackApi1 callback = new IMEInput(this, generation, serial);
        try {
            IBinder binder = agent.getCurrentIMEAgent();
            if (binder == null) return;
            IIMEServiceAgent serviceAgent = IIMEServiceAgent.Stub.asInterface(binder);
            if (serviceAgent == null) return;
            service = IIMEServiceApi1.Stub.asInterface(serviceAgent.getApi("UnityPlugin_IMEService", 1));
            if (service == null) return;
            death = new IMEDeath(this, generation, binder, 1);
            binder.linkToDeath(death, 0);
            boolean current;
            synchronized (IMEManager.class) {
                current = p2Current(generation) && mP2ServiceGeneration == serial && mIMEManagerAgaent == agent && death.binder != null;
                if (current) { mP2ServiceDeath = death; IMEService = service; imeClientCB = callback; connectState = 2; }
            }
            if (!current) { death.close(); return; }
            service.addCallback(callback);
            synchronized (IMEManager.class) { current = p2Current(generation) && IMEService == service && mP2ServiceGeneration == serial; }
            if (!current) new IMECleanup(null, null, null, service, callback, null, death).close();
            else synchronized (IMEManager.class) { if (p2Current(generation) && IMEService == service && mP2ServiceGeneration == serial) mP2HandshakeFailures = 0; }
        } catch (RemoteException | RuntimeException failure) {
            boolean retry = false;
            synchronized (IMEManager.class) {
                if (IMEService == service && mP2ServiceGeneration == serial) {
                    IMEService = null; mP2ServiceDeath = null; connectState = 0; ++mP2ServiceGeneration;
                    retry = p2Current(generation);
                }
            }
            new IMECleanup(null, null, null, service, callback, null, death).close();
            Log.e("Java_IMEManager", "IME service failed", failure);
            if (retry) p2ScheduleRetry(generation);
        }
    }

    private IMECleanup p2Detach() {
        IMECleanup cleanup = new IMECleanup(mP2Binding, IIMEManager, imeManagerClientCB, IMEService, imeClientCB, mP2ManagerDeath, mP2ServiceDeath);
        mP2Binding = null; IIMEManager = null; IMEService = null; mIMEManagerAgaent = null;
        mP2ManagerDeath = null; mP2ServiceDeath = null;
        managerConnectState = 0; connectState = 0; isIMEManagerExist = false;
        ++mP2ServiceGeneration;
        return cleanup;
    }

    private void disconnectIMEManager() {
        IMECleanup cleanup;
        synchronized (IMEManager.class) { cleanup = p2Detach(); }
        cleanup.close();
    }

    void p2Died(IMEDeath death) {
        IMECleanup cleanup;
        boolean manager;
        synchronized (IMEManager.class) {
            if (!p2Current(death.generation)) return;
            manager = death.type == 0;
            if (manager) { if (mP2ManagerDeath != death) return; cleanup = p2Detach(); }
            else {
                if (mP2ServiceDeath != death) return;
                cleanup = new IMECleanup(null, null, null, IMEService, imeClientCB, null, mP2ServiceDeath);
                IMEService = null; mP2ServiceDeath = null; connectState = 0; mShowOnVRScene = false; ++mP2ServiceGeneration;
            }
        }
        cleanup.close();
        p2ScheduleRetry(death.generation);
    }

    void p2Switch(IMEBinding binding) {
        p2ConnectService(binding);
    }

    public void hideKeyboard() {
        IIMEServiceApi1 service;
        synchronized (IMEManager.class) {
            if (!mP2Active || connectState != 2 || IMEService == null) return;
            mShowOnVRScene = false; service = IMEService;
        }
        try { service.hideKeyboard(); }
        catch (RemoteException | RuntimeException failure) { Log.e("Java_IMEManager", "Hide keyboard failed", failure); }
    }

    void p2Input(long generation, long serviceGeneration, int event, int id, String content, int value) {
        IMECallback callback = null;
        InputResult result = null;
        boolean complete = false;
        synchronized (IMEManager.class) {
            if (!p2Current(generation) || serviceGeneration != mP2ServiceGeneration) return;
            if (event == 0) {
                mId = id; mInputContent = content; mShowOnVRScene = false;
                if (isInProcess) {
                    complete = true; callback = mCallback; result = new InputResult(mId, mInputContent, mErrorCode);
                }
            }
            else if (event == 1) { mErrorCode = value; mShowOnVRScene = false; }
            else if (event == 2) mShowOnVRScene = false;
            else if (isInProcess && mCallback != null) { callback = mCallback; result = new InputResult(mId, content, value, mErrorCode); }
        }
        if (callback != null) { if (complete) callback.onInputCompletedwithObject(result); else callback.onInputClickedwithObject(result); }
    }

    public void requestIMECompleted() {
        long generation, serviceGeneration;
        synchronized (IMEManager.class) { generation = mP2Generation; serviceGeneration = mP2ServiceGeneration; }
        p2Complete(generation, serviceGeneration);
    }

    private void p2Complete(long generation, long serviceGeneration) {
        IMECallback callback;
        InputResult result;
        synchronized (IMEManager.class) {
            if (!p2Current(generation) || mP2ServiceGeneration != serviceGeneration) return;
            callback = mCallback; result = new InputResult(mId, mInputContent, mErrorCode);
        }
        if (callback != null) callback.onInputCompletedwithObject(result);
    }

    public void showKeyboard(IMEParameter parameter, IMECallback callback) { showKeyboard(parameter, callback, true); }
    public void showKeyboard(IMEParameter parameter, IMECallback callback, boolean editor) {
        long generation, request;
        synchronized (IMEManager.class) {
            if (!mP2Active) return;
            generation = mP2Generation; request = ++mP2InputRequest;
            mParameter = parameter; mCallback = callback; isInProcess = true;
        }
        int error = p2Show(parameter, editor, generation, request);
        synchronized (IMEManager.class) { if (p2Current(generation) && mP2InputRequest == request) mErrorCode = error; }
    }
    public int showKeyboardDefault(IMEParameter parameter, IMECallback callback) { return showKeyboardDefault(parameter, callback, true); }
    public int showKeyboardDefault(IMEParameter parameter, IMECallback callback, boolean editor) {
        long generation, request;
        synchronized (IMEManager.class) {
            if (!mP2Active) return 0;
            generation = mP2Generation; request = ++mP2InputRequest; mCallback = callback;
        }
        return p2Show(parameter, editor, generation, request);
    }

    private int p2Show(IMEParameter parameter, boolean editor, long generation, long request) {
        IIMEServiceApi1 service;
        synchronized (IMEManager.class) {
            if (!p2Current(generation) || mP2InputRequest != request) return 0;
            if (connectState != 2 || IMEService == null) return 0;
            mShowOnVRScene = true; service = IMEService;
        }
        try { return !editor && service.isSupportEnableEditorPanel() ? service.showKeyboardNoEditorPanel(parameter) : service.showKeyboard(parameter); }
        catch (RemoteException failure) { Log.e("Java_IMEManager", "Show keyboard failed", failure); return 0; }
    }

    public void restoreShowKeyboard() {
        IMEParameter parameter; long generation, request;
        synchronized (IMEManager.class) {
            if (!mP2Active || mParameter == null) return;
            parameter = mParameter; generation = mP2Generation; request = ++mP2InputRequest;
        }
        p2Show(parameter, true, generation, request);
        synchronized (IMEManager.class) { if (p2Current(generation) && mP2InputRequest == request) { isInProcess = true; mShowOnVRScene = true; } }
    }
}
