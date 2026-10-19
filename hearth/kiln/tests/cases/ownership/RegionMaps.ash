// HashMap / HashSet declared in the body of a loop (jown/region.c): the table, its entries, the lists made by
// keys() / values() / toList() and the temporaries of hashing all die with the iteration, so each loop below is a
// region and must print what ashvm prints with the reclaimed bytes poisoned. Keys and values are Strings or numbers.
import ash.util.ArrayList;
import ash.util.HashMap;
import ash.util.HashSet;
import ash.util.List;
import ash.util.Map;
import ash.util.Set;

public class RegionMaps {
    static String[] words = {"red", "green", "blue", "green", "red", "red", "yellow"};

    static long countWords(int rounds) {
        long total = 0;
        for (int r = 0; r < rounds; r++) {
            Map<String, Integer> counts = new HashMap<>();
            for (String w : words) {
                String k = w + (r % 2);
                Integer? c = counts.get(k);
                counts.put(k, c == null ? 1 : c + 1);
            }
            total += counts.size() * 100;
            int best = 0;
            for (String k : counts.keys()) {
                Integer? c = counts.get(k);
                if (c != null && c > best) best = c;
            }
            total += best;
            if (r % 3 == 0) System.out.println("round " + r + " size " + counts.size() + " best " + best + " red? " + counts.containsKey("red" + (r % 2)));
        }
        return total;
    }

    static long distinct(int rounds) {
        long total = 0;
        for (int r = 0; r < rounds; r++) {
            Set<Integer> seen = new HashSet<>();
            int dups = 0;
            for (int i = 0; i < 12; i++) {
                int v = (i * 7 + r) % 5;
                if (!seen.add(v)) dups++;
            }
            total += seen.size() * 10 + dups;
            if (seen.contains(3)) total += 1000;
            seen.remove(3);
            total += seen.size();
            for (int v : seen.toList()) total += v;
        }
        return total;
    }

    static long numbersAndStrings(int rounds) {
        long total = 0;
        for (int r = 0; r < rounds; r++) {
            HashMap<Integer, String> names = new HashMap<>();
            for (int i = 0; i < 5; i++) names.put(i * r, "n" + i + "_" + r);
            names.put(0, "zero" + r);
            String? got = names.get(r * 2);
            if (got != null) total += got.length();
            String? removed = names.remove(0);
            if (removed != null) total += removed.length();
            for (String v : names.values()) total += v.length();
            List<Integer> ks = names.keys();
            for (int k : ks) total += k;
        }
        return total;
    }

    static long setOfWords(int rounds) {
        long total = 0;
        for (int r = 0; r < rounds; r++) {
            HashSet<String> uniq = new HashSet<>();
            for (String w : words) uniq.add(w + r);
            uniq.add("red" + r);
            total += uniq.size();
            if (uniq.contains("blue" + r)) total += 50;
            if (uniq.isEmpty()) total += 99999;
            String longest = "";
            for (String u : uniq.toList()) if (u.length() > longest.length()) longest = u;
            total += longest.length();
        }
        return total;
    }

    static long reassigned(int rounds) {
        long total = 0;
        for (int r = 0; r < rounds; r++) {
            Map<String, Integer> m = new HashMap<>();
            m.put("a" + r, r);
            m = new HashMap<>();
            m.put("b" + r, r * 2);
            for (String k : m.keys()) {
                Integer? v = m.get(k);
                if (v != null) total += k.length() + v;
            }
        }
        return total;
    }

    static long earlyExits(int rounds) {
        long total = 0;
        for (int r = 0; r < rounds; r++) {
            Map<String, Integer> m = new HashMap<>();
            for (int i = 0; i < 6; i++) {
                String k = "k" + i;
                if (i == 4) break;
                if (i == 1) continue;
                m.put(k, i);
            }
            total += m.size();
            Integer? k0 = m.get("k0");
            if (r == rounds - 1 && k0 != null) return total + k0;
        }
        return total;
    }

    static long afterLoop() {
        long total = countWords(4);
        String keep = "after-" + total;
        Map<String, Integer> outside = new HashMap<>();
        for (int i = 0; i < 5; i++) {
            Map<String, Integer> tmp = new HashMap<>();
            tmp.put("x" + i, i);
            Integer? t = tmp.get("x" + i);
            if (t != null) total += t;
        }
        outside.put(keep, 1);
        Integer? o = outside.get(keep);
        return total + (o == null ? 0 : o) + keep.length();
    }

    public static void main(String[] args) {
        System.out.println(countWords(7));
        System.out.println(distinct(6));
        System.out.println(numbersAndStrings(5));
        System.out.println(setOfWords(4));
        System.out.println(reassigned(5));
        System.out.println(earlyExits(4));
        System.out.println(afterLoop());
    }
}
