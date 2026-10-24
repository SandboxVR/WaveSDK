package android.os;
public interface IBinder {
    interface DeathRecipient { void binderDied(); }
    IInterface queryLocalInterface(String descriptor);
    void linkToDeath(DeathRecipient recipient, int flags) throws RemoteException;
    boolean unlinkToDeath(DeathRecipient recipient, int flags);
    boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException;
}
