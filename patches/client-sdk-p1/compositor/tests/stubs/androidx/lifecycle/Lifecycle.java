package androidx.lifecycle;
public class Lifecycle {
    public enum State { DESTROYED, CREATED, RESUMED }
    public State state = State.CREATED;
    public State getCurrentState() { return state; }
}
