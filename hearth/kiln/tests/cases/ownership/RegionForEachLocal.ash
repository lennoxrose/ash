// for-each loops over lists (and arrays) declared in the body of a region loop (jown/region.c): the list and
// its elements die with the iteration, and a list is read by index on every step, so growing it while it is
// walked (a ConcurrentModificationException, caught in the iteration) leaves nothing stale behind. Every loop
// counted by region_check.sh must be a region and print what ashvm prints with the reclaimed bytes poisoned.
import ash.util.ArrayList;
import ash.util.ConcurrentModificationException;
import ash.util.List;

public class RegionForEachLocal {
    static long walkOwn(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            List<String> parts = new ArrayList<>();
            parts.add("a" + i);
            parts.add("bb" + i);
            parts.add("ccc" + i);
            for (String p : parts) {
                String q = p + "!" + p.length();
                total += q.length();
            }
            if (i % 3 == 0) System.out.println("i=" + i + " parts=" + parts.size() + " first=" + parts.get(0));
        }
        return total;
    }

    static long numbers(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            List<Integer> xs = new ArrayList<>();
            for (int k = 0; k <= i; k++) xs.add(k * k);
            for (int x : xs) {
                String s = "x" + x;
                total += s.length() + x;
            }
            int[] local = new int[3];
            local[1] = i;
            for (int v : local) {
                String t = "v" + v + i;
                total += t.length();
            }
        }
        return total;
    }

    static long growWhileWalking(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            List<String> xs = new ArrayList<>();
            xs.add("s" + i);
            xs.add("t" + i);
            try {
                for (String x : xs) {
                    String y = x + x;
                    total += y.length();
                    xs.add(y);
                }
                total += 1000;
            } catch (ConcurrentModificationException e) {
                total += 7 + xs.size();
            }
        }
        return total;
    }

    static long removeWhileWalking(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            List<String> xs = new ArrayList<>();
            for (int k = 0; k < 4; k++) xs.add("k" + k + i);
            for (String x : xs) {
                String y = x + "?";
                total += y.length();
                if (x.startsWith("k2")) xs.remove(0);      // the position equals the new size: the loop ends
            }
            total += xs.size();
        }
        return total;
    }

    static long breakOut(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            List<String> xs = new ArrayList<>();
            for (int k = 0; k < 5; k++) xs.add("e" + k);
            for (String x : xs) {
                String y = x + i;
                if (y.startsWith("e3")) { xs.add(y); break; }
                if (y.startsWith("e1")) continue;
                total += y.length();
            }
            total += xs.size();
        }
        return total;
    }

    public static void main(String[] args) {
        System.out.println(walkOwn(7));
        System.out.println(numbers(6));
        System.out.println(growWhileWalking(4));
        System.out.println(removeWhileWalking(5));
        System.out.println(breakOut(5));
    }
}
