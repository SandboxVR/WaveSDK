package android.os;
import java.io.Serializable;
import java.util.HashMap;
public class Bundle {
    private final HashMap<String, Object> values = new HashMap<String, Object>();
    public void putSerializable(String key, Serializable value) { values.put(key, value); }
    public Serializable getSerializable(String key) { return (Serializable) values.get(key); }
}
