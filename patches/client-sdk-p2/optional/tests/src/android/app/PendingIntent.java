package android.app;
import android.content.*;
public class PendingIntent {
    public final Intent intent; public boolean cancelled;
    public PendingIntent(Intent intent) { this.intent = intent; }
    public static PendingIntent getBroadcast(Context context, int code, Intent intent, int flags) { return new PendingIntent(intent); }
    public void cancel() { cancelled = true; }
}
