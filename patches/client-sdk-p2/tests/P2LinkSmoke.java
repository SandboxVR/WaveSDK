/** Verify the patched class/member graph in ART without starting optional VR features. */
public final class P2LinkSmoke {
    public static void main(String[] names) throws Exception {
        int count=0;
        for(String name:names){
            Class<?> c=Class.forName(name,false,P2LinkSmoke.class.getClassLoader());
            c.getDeclaredFields();
            c.getDeclaredMethods();
            c.getDeclaredConstructors();
            count++;
        }
        if(count==0)throw new AssertionError("No patch classes supplied");
        System.out.println("PASS P2 ART class/member resolution: "+count+" classes; optional services not activated");
    }
}
