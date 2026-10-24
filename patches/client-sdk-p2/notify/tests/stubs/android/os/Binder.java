package android.os;
public class Binder implements IBinder {
    public DeathRecipient recipient;
    public int unlinks;
    public void linkToDeath(DeathRecipient value, int flags) throws RemoteException {
        recipient = value;
    }
    public boolean unlinkToDeath(DeathRecipient value, int flags) {
        unlinks++;
        if (recipient == value) recipient = null;
        return true;
    }
    public void die() {
        DeathRecipient value = recipient;
        if (value != null) value.binderDied();
    }
}
