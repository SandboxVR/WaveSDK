package kotlin.jvm.internal;
public final class Intrinsics {
    public static void checkNotNullParameter(Object value, String name) {
        if (value == null) throw new NullPointerException(name);
    }
}
