package android.hardware.usb;
import android.os.*;
public class UsbDevice implements Parcelable {
    public final String name; public UsbDevice(String name) { this.name = name; }
    public String getProductName() { return name; }
    public int describeContents() { return 0; }
    public void writeToParcel(Parcel parcel, int flags) { }
    public static final Parcelable.Creator<UsbDevice> CREATOR = new Parcelable.Creator<UsbDevice>() { public UsbDevice createFromParcel(Parcel parcel) { return new UsbDevice("parcel"); } public UsbDevice[] newArray(int size) { return new UsbDevice[size]; } };
}
