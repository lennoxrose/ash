public class OwnOk {
    static int len(String s) { return s.length(); }

    public static void main(String[] args) {
        String a = "hello";
        int n = len(&a) + len(&a);        // &a lends it: a stays usable
        String b = a;                     // a moved into b
        a = "again";                      // reinitialised: usable again
        System.out.println(n + b + a);

        String c = "x";
        if (n > 100) {
            String d = c;                 // moved on a path that returns
            System.out.println(d);
            return;
        }
        System.out.println(c);

        for (int i = 0; i < 3; i++) {
            String e = "iter" + i;        // a fresh variable every iteration
            String f = e;
            System.out.println(f);
        }
        String g = "g";
        while (true) {
            String h = g;                 // moved, then the loop is left
            System.out.println(h);
            break;
        }
    }
}
