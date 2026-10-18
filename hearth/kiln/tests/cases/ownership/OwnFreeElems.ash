import ash.util.ArrayList;
import ash.util.List;

public class OwnFreeElems {
    static class Bag {
        List<String> names = new ArrayList<>();    // owned, only fresh adds, only harmless reads: its Strings die with the Bag
        void add(int i) { names.add("n" + i); }
        int total() {
            int t = 0;
            for (int i = 0; i < names.size(); i++) t += names.get(i).length();
            return t;
        }
    }

    static String last;                            // keeps an element alive past its list

    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 20000; i++) {
            List<String> a = new ArrayList<>();    // fresh adds + harmless reads: elements freed with the list
            a.add("a" + i);
            a.add("b" + i);
            total += a.get(0).length() + a.size();
            if (i % 10000 == 0) System.out.println(a.get(1));
            for (String s : a) total += s.length();       // the loop variable is only read

            List<String> b = new ArrayList<>();    // a literal element: the list is freed, the literal never is
            b.add("lit");
            b.add("v" + i);
            total += b.get(0).length();

            List<String> c = new ArrayList<>();    // an element escapes through a local alias
            c.add("c" + i);
            String kept = c.get(0);
            if (i == 7) last = kept;

            String[] d = new String[2];            // fresh stores only
            d[0] = "d" + i;
            d[1] = "e" + i;
            total += d[0].length() + d[1].length();

            String[] e = new String[2];            // a shared store
            String shared = "shared" + i;
            total += shared.length();
            e[0] = shared;
            e[1] = "f" + i;
            total += e[0].length();

            List<String> f = new ArrayList<>();    // loop variable escapes
            f.add("g" + i);
            for (String s : f) { if (i == 9) last = s; }

            Bag bag = new Bag();
            bag.add(i);
            bag.add(2);
            total += bag.total();
        }
        System.out.println(total + " " + last);
    }
}
