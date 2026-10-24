package android.os;
import java.util.*;
public class Handler {
    public final List<Runnable> pending = new ArrayList<Runnable>();
    public Handler(Looper looper) { }
    public boolean post(Runnable work) { return postDelayed(work, 0L); }
    public boolean postDelayed(Runnable work, long delay) { pending.add(work); return true; }
    public void removeCallbacks(Runnable work) { for (Iterator<Runnable> it = pending.iterator(); it.hasNext();) if (it.next() == work) it.remove(); }
    public Runnable take() { return pending.remove(0); }
}
