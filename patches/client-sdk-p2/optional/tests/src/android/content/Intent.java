package android.content;
import android.os.Parcelable;
import java.util.*;
public class Intent {
    private String action, packageName; private ComponentName component;
    private final Map<String,Object> extras = new HashMap<String,Object>();
    public Intent() { }
    public Intent(String action) { this.action = action; }
    public Intent(Intent intent) { action = intent.action; packageName = intent.packageName; component = intent.component; }
    public String getAction() { return action; }
    public Intent setAction(String action) { this.action = action; return this; }
    public Intent setPackage(String name) { packageName = name; return this; }
    public Intent setComponent(ComponentName name) { component = name; return this; }
    public Intent putExtra(String key, Parcelable value) { extras.put(key, value); return this; }
    public Intent putExtra(String key, boolean value) { extras.put(key, value); return this; }
    public Parcelable getParcelableExtra(String key) { return (Parcelable) extras.get(key); }
    public boolean getBooleanExtra(String key, boolean fallback) { Object value = extras.get(key); return value instanceof Boolean ? (Boolean) value : fallback; }
}
