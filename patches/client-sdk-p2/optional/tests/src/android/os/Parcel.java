package android.os;
public class Parcel {
    public static Parcel obtain() { return new Parcel(); }
    public void recycle() { }
    public void enforceInterface(String descriptor) { }
    public void writeInterfaceToken(String descriptor) { }
    public void writeNoException() { }
    public void readException() { }
    public int readInt() { return 0; }
    public String readString() { return null; }
    public IBinder readStrongBinder() { return null; }
    public void writeInt(int value) { }
    public void writeString(String value) { }
    public void writeStrongBinder(IBinder value) { }
    public void writeStringArray(String[] value) { }
    public String[] createStringArray() { return null; }
    public void writeIntArray(int[] value) { }
    public int[] createIntArray() { return null; }
    public void writeDoubleArray(double[] value) { }
    public double[] createDoubleArray() { return null; }
    public void writeLong(long value) { }
    public long readLong() { return 0; }
    public void writeFloat(float value) { }
    public float readFloat() { return 0; }
    public <T extends Parcelable> void writeTypedArray(T[] value, int flags) { }
    public <T> T[] createTypedArray(Parcelable.Creator<T> creator) { return creator.newArray(0); }
}
