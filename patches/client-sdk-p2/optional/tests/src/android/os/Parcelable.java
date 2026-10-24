package android.os;
public interface Parcelable { interface Creator<T> { T createFromParcel(Parcel parcel); T[] newArray(int size); } int describeContents(); void writeToParcel(Parcel parcel, int flags); }
