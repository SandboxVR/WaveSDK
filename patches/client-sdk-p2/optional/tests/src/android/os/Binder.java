package android.os;
import java.util.*;
public class Binder implements IBinder {
    public final List<DeathRecipient> deaths = new ArrayList<DeathRecipient>();
    private IInterface local; private String descriptor;
    public void attachInterface(IInterface local, String descriptor) { this.local = local; this.descriptor = descriptor; }
    public IInterface queryLocalInterface(String descriptor) { return descriptor.equals(this.descriptor) ? local : null; }
    public IBinder asBinder() { return this; }
    public void linkToDeath(DeathRecipient recipient, int flags) { deaths.add(recipient); }
    public boolean unlinkToDeath(DeathRecipient recipient, int flags) { return deaths.remove(recipient); }
    public boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException { return onTransact(code, data, reply, flags); }
    protected boolean onTransact(int code, Parcel data, Parcel reply, int flags) throws RemoteException { return false; }
}
