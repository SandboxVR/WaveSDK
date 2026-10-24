package android.graphics;
import android.os.*;
public class Bitmap implements Parcelable {
    public int describeContents() { return 0; }
    public void writeToParcel(Parcel parcel, int flags) { }
    public static final Parcelable.Creator<Bitmap> CREATOR = new Parcelable.Creator<Bitmap>() { public Bitmap createFromParcel(Parcel parcel) { return new Bitmap(); } public Bitmap[] newArray(int size) { return new Bitmap[size]; } };
}
