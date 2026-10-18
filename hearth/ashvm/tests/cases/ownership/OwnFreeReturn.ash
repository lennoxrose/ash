public class OwnFreeReturn {
    static String label(int i) {
        String s = "n" + i;               // sole owner handed to the caller by `return s`
        return s;
    }

    static String pick(int i) {
        if (i % 2 == 0) return "even" + i;
        return "odd" + i;                 // both returns are fresh
    }

    static String name = "shared";
    static String alias(int i) { return name; }   // not fresh: callers must never free it

    static int[] table(int n) {
        int[] t = new int[n];
        t[0] = n;
        return t;
    }

    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 200000; i++) {
            String a = label(i);          // fresh result: sole owner, freed each iteration
            String b = pick(i);
            String c = alias(i);          // aliases a static: never freed
            int[] t = table(8);
            total += a.length() + b.length() + c.length() + t[0];
        }
        System.out.println(total + " " + name);
    }
}
