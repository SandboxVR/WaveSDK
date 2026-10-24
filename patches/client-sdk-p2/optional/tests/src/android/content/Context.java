package android.content;
import android.content.pm.PackageManager;
import android.content.res.Resources;
public abstract class Context {
    public static final int BIND_AUTO_CREATE = 1;
    public static final String USB_SERVICE = "usb";
    public abstract Context getApplicationContext();
    public abstract boolean bindService(Intent intent, ServiceConnection connection, int flags);
    public abstract void unbindService(ServiceConnection connection);
    public abstract PackageManager getPackageManager();
    public abstract String getPackageName();
    public abstract Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter);
    public abstract void unregisterReceiver(BroadcastReceiver receiver);
    public Object getSystemService(String name) { return null; }
    public Resources getResources() { return new Resources(); }
}
