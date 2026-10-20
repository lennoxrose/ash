// Loops with a HashMap / HashSet in them that must NOT be regions (jown/region.c): the map, its entries or a list
// made from it outlive the iteration, or hashing / iterating it would run project code.
import ash.util.ArrayList;
import ash.util.HashMap;
import ash.util.HashSet;
import ash.util.List;
import ash.util.Map;
import ash.util.Set;

public class RegionMapEscapes {
    static Map<String, Integer> kept;
    static List<String> savedKeys;
    static String[] words = {"red", "green", "blue", "green", "red"};

    static class Item {
        final String name;
        Item(String name) { this.name = name; }
        public int hashCode() { return name.length(); }
        public bool equals(Object o) { return o instanceof Item && ((Item) o).name.length() == name.length(); }
    }

    static int val(Map<String, Integer> m, String k) {
        Integer? v = m.get(k);
        return v == null ? 0 : v;
    }

    static void keep(Map<String, Integer> m) { kept = m; }

    static long outerMap() {
        Map<String, Integer> counts = new HashMap<>();
        for (String w : words) {
            String k = w + "!";
            Integer? c = counts.get(k);
            counts.put(k, c == null ? 1 : c + 1);
        }
        return counts.size() * 100 + val(counts, "red!");
    }

    static long staticKeeps() {
        long total = 0;
        for (String w : words) {
            Map<String, Integer> m = new HashMap<>();
            m.put(w + "?", w.length());
            total += m.size();
            kept = m;
        }
        return total + kept.size() + val(kept, "red?");
    }

    static long callKeeps() {
        long total = 0;
        for (String w : words) {
            Map<String, Integer> m = new HashMap<>();
            m.put(w + "#", 7);
            total += m.size();
            keep(m);
        }
        return total + val(kept, "red#");
    }

    static long listKeeps() {
        long total = 0;
        for (String w : words) {
            Map<String, Integer> m = new HashMap<>();
            m.put(w + "@", 1);
            savedKeys = m.keys();
            total += m.size();
        }
        return total + savedKeys.get(0).length();
    }

    static long aliasKept() {
        Map<String, Integer> last = new HashMap<>();
        for (String w : words) {
            Map<String, Integer> m = new HashMap<>();
            m.put(w + "~", w.length());
            last = m;
        }
        return val(last, "red~");
    }

    static long arrayValues() {
        long total = 0;
        for (String w : words) {
            Map<String, int[]> m = new HashMap<>();
            int[] a = new int[3];
            a[1] = w.length();
            m.put(w, a);
            int[]? got = m.get(w);
            if (got != null) total += got[1];
        }
        return total;
    }

    static long objectKeys() {
        long total = 0;
        for (String w : words) {
            Set<Item> items = new HashSet<>();
            items.add(new Item("i" + w));
            items.add(new Item(w + "x"));
            items.add(new Item("zzzzz"));
            total += items.size();
        }
        return total;
    }

    static long setForEach() {
        long total = 0;
        for (String w : words) {
            Set<String> s = new HashSet<>();
            s.add(w + "1");
            s.add(w + "2");
            for (String x : s) total += x.length();
        }
        return total;
    }

    static long mapInsideTry() {
        long total = 0;
        for (String w : words) {
            Map<String, Integer> m = new HashMap<>();
            m.put(w, w.length());
            keep(m);
            total += 1;
        }
        return total + kept.size();
    }

    public static void main(String[] args) {
        System.out.println(outerMap());
        System.out.println(staticKeeps());
        System.out.println(callKeeps());
        System.out.println(listKeeps());
        System.out.println(aliasKept());
        System.out.println(arrayValues());
        System.out.println(objectKeys());
        System.out.println(setForEach());
        System.out.println(mapInsideTry());
    }
}
