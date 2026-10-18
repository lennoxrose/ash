import ash.util.ArrayList;
import ash.util.HashMap;
import ash.util.HashSet;
import ash.util.List;
import ash.util.Map;
import ash.util.Set;

public class OwnFreeMaps {
    static class Index {
        Map<String, Integer> counts = new HashMap<>();   // owned field
        void see(String w) {
            Integer? n = counts.get(w);
            counts.put(w, n == null ? 1 : n + 1);
        }
        int distinct() { return counts.size(); }
    }

    static Map<String, Integer> shared = new HashMap<>();

    static int lookup(Map<String, Integer> m, String k) { return m.containsKey(k) ? 1 : 0; }   // reads only

    public static void main(String[] args) {
        long total = 0;
        Map<String, Integer> kept = new HashMap<>();
        for (int i = 0; i < 5000; i++) {
            Map<String, Integer> m = new HashMap<>();      // sole owner: freed with its entry chains each iteration
            m.put("a" + i, i);
            m.put("b", 2);
            m.put("b", 3);
            total += m.size() + (m.containsKey("b") ? 1 : 0);
            if (m.containsKey("a" + i)) total += 1;
            m.remove("b");
            List<String> ks = m.keys();
            total += ks.size();
            Set<Integer> s = new HashSet<>();
            s.add(i % 7);
            s.add(3);
            s.add(3);
            total += s.size();
            if (s.contains(3)) total += 1;
            total += lookup(m, "a" + i);                   // a callee that only reads: still a sole owner
            Index ix = new Index();
            ix.see("x");
            ix.see("x");
            ix.see("y");
            total += ix.distinct();
            Map<String, Integer> k2 = new HashMap<>();
            k2.put("k", i);
            if (i == 4) kept = k2;                         // escapes: not freed
            if (i == 5) shared = m;
        }
        System.out.println(total + " " + kept.get("k") + " " + shared.size());
    }
}
