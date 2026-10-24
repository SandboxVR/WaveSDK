package android.app;
import android.content.*;
import android.content.pm.PackageManager;
public class Activity extends Context {
    public final Context application;
    public int requested;
    public Activity(Context application) { this.application = application; }
    public Context getApplicationContext() { return application; }
    public boolean bindService(Intent intent, ServiceConnection connection, int flags) { throw new AssertionError("Activity must not own service binding"); }
    public void unbindService(ServiceConnection connection) { throw new AssertionError("Activity must not own service binding"); }
    public PackageManager getPackageManager() { return application.getPackageManager(); }
    public String getPackageName() { return "host.app"; }
    public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) { throw new AssertionError("Activity must not own receiver"); }
    public void unregisterReceiver(BroadcastReceiver receiver) { throw new AssertionError("Activity must not own receiver"); }
    public Object getSystemService(String name) { return application.getSystemService(name); }
    public void requestPermissions(String[] names, int code) { requested++; }
}
