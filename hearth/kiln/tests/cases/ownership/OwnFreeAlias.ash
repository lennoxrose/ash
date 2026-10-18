import ash.util.ArrayList;
import ash.util.List;

public class OwnFreeAlias {
    static class A {
        String get(int i) { return "a" + i; }          // fresh here...
    }
    static class B extends A {
        String shared = "shared";
        String get(int i) { return shared; }           // ...but an override returns an alias: calls through A are never fresh
    }

    static class Box {
        String held;
        void keep(String s) { held = s; }              // keeps its argument
    }

    static String grow(int n) {
        if (n == 0) return "";
        String rest = grow(n - 1);                     // recursive: never treated as fresh
        return rest + n;
    }

    public static void main(String[] args) {
        long total = 0;
        List<String> kept = new ArrayList<>();
        Box box = new Box();
        for (int i = 0; i < 20000; i++) {
            A a = (i % 2 == 0) ? new A() : new B();
            String g = a.get(i);                       // virtual: must stay alive when it is B's field
            String t = "t" + i;
            String u = "u" + i;
            bool same = t.equals(u) || t.contains("1");   // reads only: t and u stay sole owners
            String v = (i % 3 == 0) ? t : u;           // aliases: t and u must not be freed
            kept.add("k" + i);                         // fresh temp handed to a collection: never freed here
            if (i % 5000 == 0) box.keep("b" + i);      // the argument outlives the iteration
            String w = grow(3);
            total += g.length() + v.length() + (same ? 1 : 0) + w.length();
            int[] row = new int[4];
            for (int x : row) total += x;              // for-each over a sole-owner array
            switch (i % 3) {
                case 0 -> { String z = "z" + i; total += z.length(); }
                case 1 -> { int[] y = new int[2]; total += y.length; }
                default -> total += 1;
            }
        }
        System.out.println(total + " " + kept.size() + " " + kept.get(19999) + " " + box.held + " " + new B().shared);
    }
}
