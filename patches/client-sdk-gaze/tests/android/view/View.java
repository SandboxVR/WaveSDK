package android.view;
public class View {
    public View child;
    public View findViewById(int id) { return child; }
    public void measure(int width, int height) { }
    public int getMeasuredWidth() { return 24; }
    public int getMeasuredHeight() { return 24; }
    public static class MeasureSpec {
        public static int makeMeasureSpec(int size, int mode) { return size | mode; }
    }
}
