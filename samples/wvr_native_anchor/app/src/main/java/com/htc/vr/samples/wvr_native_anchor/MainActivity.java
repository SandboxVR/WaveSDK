// "WaveVR SDK 
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."

package com.htc.vr.samples.wvr_native_anchor;

import com.htc.vr.sdk.VRActivity;

import android.Manifest;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.annotation.TargetApi;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.util.Log;
import android.content.DialogInterface;

public class MainActivity extends VRActivity {
    private static final String TAG = "wvr_native_anchor";

    private static final String ACTION_SWITCH_DEBUG = "com.htc.vr.samples.wvr_native_anchor.ACTION_SWITCH_DEBUG";
    private static final String ACTION_SWITCH_MSAA = "com.htc.vr.samples.wvr_native_anchor.ACTION_SWITCH_MSAA";
    private static final String ACTION_SWITCH_SCENE = "com.htc.vr.samples.wvr_native_anchor.ACTION_SWITCH_SCENE";
    public static final String SCENE_PERMISSION = "wave.permission.GET_SCENE_MESH";
    private static final String SP_DEBUG = "debug";
    private static final int FLAG_DEBUG = 0x01;
    private static final int FLAG_MSAA = 0x02;
    private static final int FLAG_SCENE = 0x04;
    private boolean mDebug = false;
    private int mFlag = FLAG_MSAA;
    private final String CHANNEL_ID = "channel.id.wvr_native_anchor";
    private static int REQUEST_CODE=123;

    // The scene mesh permission.
    // need the permission to get scene plane and scene mesh work.
    private boolean mHasPermission = false;

    static {
        System.loadLibrary("nativeAnchor_jni");
    }
    public MainActivity() {
    }

    private BroadcastReceiver receiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.i(TAG,"onReceive: start");
            if (intent.getAction().equals(ACTION_SWITCH_DEBUG)) {
                mDebug = !mDebug;
                SharedPreferences p = PreferenceManager.getDefaultSharedPreferences(getApplicationContext());
                p.edit().putBoolean(SP_DEBUG, mDebug).commit();
                mFlag = (mFlag & ~FLAG_DEBUG) | (mDebug ? FLAG_DEBUG : 0);
            } else if (intent.getAction().equals(ACTION_SWITCH_MSAA)) {
                boolean msaa = (mFlag & FLAG_MSAA) == 0;  // invert flag value here
                mFlag = (mFlag & ~FLAG_MSAA) | (msaa ? FLAG_MSAA : 0);
            } else if (intent.getAction().equals(ACTION_SWITCH_SCENE)) {
                boolean scene = (mFlag & FLAG_SCENE) == 0;  // invert flag value here
                mFlag = (mFlag & ~FLAG_SCENE) | (scene ? FLAG_SCENE : 0);
            } else {
                return;
            }

            setFlag(mFlag);

            clearNotification();
            setNotification();
            Log.i(TAG,"onReceive: end");
        }
    };

    static native void setFlag(int flag);
    static native void setPermission(boolean granted);
    static native void onResumeCall();

    @Override
    protected void onCreate(Bundle icicle) {
        //check permission
        int hasPermission=checkSelfPermission(SCENE_PERMISSION);
        if(hasPermission!=PackageManager.PERMISSION_GRANTED){
            Log.i(TAG, "Permission not granted try to aquire: "+SCENE_PERMISSION);
            requestPermissions(new String[]{SCENE_PERMISSION},REQUEST_CODE);
        }else{
            Log.i(TAG, "Permission granted: "+SCENE_PERMISSION);
            mHasPermission=true;
        }

        //send permission status to native
        setPermission(mHasPermission);

        Log.i(TAG,"onCreate:call init");
        init(getResources().getAssets());
        super.onCreate(icicle);

        // dump verion information
        try {
            PackageManager pm = getPackageManager();
            PackageInfo info = pm.getPackageInfo(getApplicationInfo().packageName, 0);
            Log.i(TAG, "Native Anchor version name: " + info.versionName + " code: " + info.versionCode);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }


        SharedPreferences p = PreferenceManager.getDefaultSharedPreferences(this);
        mDebug = p.getBoolean(SP_DEBUG, false);
        mFlag |= mDebug ? FLAG_DEBUG : 0;

        setFlag(mFlag);
        Log.i(TAG,"onCreate:end");

    }

    private void setNotification() {
    Log.i(TAG,"setNotification:start");
        PendingIntent pIntent = PendingIntent.getBroadcast(this, FLAG_DEBUG,
            new Intent(ACTION_SWITCH_DEBUG), PendingIntent.FLAG_UPDATE_CURRENT);

        int iconid = mDebug ? android.R.drawable.star_on : android.R.drawable.star_off;
        Notification.Action actionDebug = new Notification.Action.Builder(
                    iconid, mDebug ? "Debug On" : "Debug Off", pIntent).build();

        pIntent = PendingIntent.getBroadcast(this, FLAG_MSAA,
            new Intent(ACTION_SWITCH_MSAA), PendingIntent.FLAG_UPDATE_CURRENT);

        boolean msaa = (mFlag & FLAG_MSAA) != 0;
        iconid = msaa ? android.R.drawable.ic_media_next : android.R.drawable.ic_media_ff;
        Notification.Action actionMsaa = new Notification.Action.Builder(
                        iconid, msaa ? "MSAA On" : "MSAA Off", pIntent).build();

        pIntent = PendingIntent.getBroadcast(this, FLAG_SCENE,
            new Intent(ACTION_SWITCH_SCENE), PendingIntent.FLAG_UPDATE_CURRENT);

        boolean scene = (mFlag & FLAG_SCENE) != 0;
        iconid = android.R.drawable.ic_popup_sync;
        Notification.Action actionScene = new Notification.Action.Builder(
                                    iconid, "Scene", pIntent).build();

        NotificationManager notificationManager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        Notification.Builder builder  = new Notification.Builder(this);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                NotificationChannel channel = new NotificationChannel(CHANNEL_ID, "Notifications", NotificationManager.IMPORTANCE_NONE);
                notificationManager.createNotificationChannel(channel);
                builder = new Notification.Builder(this, CHANNEL_ID);
            } catch (Exception e) {
                Log.e(TAG, e.toString());
            }
        }

        Notification n = builder
                .setContentTitle("WVR Native Anchor Flag Switcher")
                .setSmallIcon(android.R.drawable.ic_menu_manage)
                .setOngoing(true)
                .addAction(actionMsaa)
                .addAction(actionScene)
                .addAction(actionDebug)
                .build();


        notificationManager.notify(0, n);
        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_SWITCH_DEBUG);
        filter.addAction(ACTION_SWITCH_MSAA);
        filter.addAction(ACTION_SWITCH_SCENE);
        registerReceiver(receiver, filter);
        Log.i(TAG,"setNotification:end");
    }

    private void clearNotification() {
        Log.i(TAG,"clearNotification:start");
        NotificationManager notificationManager =
                (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        notificationManager.cancelAll();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            notificationManager.deleteNotificationChannel(CHANNEL_ID);
        }
        unregisterReceiver(receiver);
        Log.i(TAG,"clearNotification:end");
    }

    @Override
    protected void onResume() {
        super.onResume();
         Log.i(TAG,"onResume:start");
        setNotification();
        onResumeCall();
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG,"onPause:start");
        clearNotification();
    }

    @Override
    protected void onDestroy(){
        super.onDestroy();
        Log.i(TAG, "onDestory");
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,  String[] permissions,  int[] grantResults) {
        if (requestCode == REQUEST_CODE && grantResults != null && grantResults.length > 0) {
            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                mHasPermission = true;
                Log.i(TAG, "Permission granted");
                //send permission status to native
                setPermission(mHasPermission);
            }else{
                Log.i(TAG, "Permission deny");
            }
        }
    }

    // Pass this acitivty instance to native
    @SuppressWarnings("unused")
    public native void init(AssetManager am);
}
