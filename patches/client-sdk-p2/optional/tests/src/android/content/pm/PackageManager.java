package android.content.pm;
import android.content.Intent;
import java.util.*;
public class PackageManager {
    public static final int PERMISSION_GRANTED = 0;
    public final List<ResolveInfo> services = new ArrayList<ResolveInfo>();
    public final List<ApplicationInfo> applications = new ArrayList<ApplicationInfo>();
    public List<ResolveInfo> queryIntentServices(Intent intent, int flags) { return services; }
    public List<ApplicationInfo> getInstalledApplications(int flags) { return applications; }
    public int checkPermission(String permission, String packageName) { return 0; }
}
