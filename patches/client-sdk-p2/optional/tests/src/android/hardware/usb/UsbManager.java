package android.hardware.usb;
import android.app.PendingIntent;
import java.util.*;
public class UsbManager {
    public static final String ACTION_USB_DEVICE_ATTACHED = "android.hardware.usb.action.USB_DEVICE_ATTACHED", ACTION_USB_DEVICE_DETACHED = "android.hardware.usb.action.USB_DEVICE_DETACHED";
    public final HashMap<String,UsbDevice> devices = new HashMap<String,UsbDevice>();
    public PendingIntent pending;
    public Runnable beforePermissionCheck;
    public boolean failRequest;
    public HashMap<String,UsbDevice> getDeviceList() { return devices; }
    public boolean hasPermission(UsbDevice device) { if (beforePermissionCheck != null) beforePermissionCheck.run(); return false; }
    public void requestPermission(UsbDevice device, PendingIntent intent) { pending = intent; if (failRequest) throw new SecurityException("host USB rejection"); }
}
