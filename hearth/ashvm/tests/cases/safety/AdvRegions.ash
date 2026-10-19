// Region loops that look innocent: strings that leave through a break, a return, a nested loop's variable, a
// pure callee, early exits, and values computed in one iteration and read after the loop.
public class AdvRegions {
    static int width(String s) { String t = s + "!"; return t.length(); }

    static String firstLong(int limit) {
        for (int i = 0; i < limit; i++) {
            String s = "item-" + i + "-" + (i * 31);
            if (s.length() > 10) return s;                 // leaves with a value built in the iteration
        }
        return "none";
    }

    static int viaBreak(int limit) {
        int found = -1;
        for (int i = 0; i < limit; i++) {
            String s = "v" + i;
            if (width(s) > 5) { found = i; break; }
        }
        return found;
    }

    static long nested() {
        long total = 0;
        bool stop = false;
        for (int i = 0; i < 300 && !stop; i++) {
            String a = "outer-" + i;
            for (int j = 0; j < 300; j++) {
                String b = a + "/" + j;
                if (b.length() > 14) break;
                total += b.length();
                if (total > 5000000) { stop = true; break; }
            }
        }
        return total;
    }

    static int[] grid(int n) {
        int[] counts = new int[4];
        for (int i = 0; i < n; i++) {
            int[] row = new int[8];
            for (int j = 0; j < 8; j++) { row[j] = i + j; counts[(i + j) % 4] += row[j] % 2; }
            String label = "r" + i;
            counts[label.length() % 4] += 1;
        }
        return counts;
    }

    public static void main(String[] args) {
        System.out.println(firstLong(1000));
        System.out.println(viaBreak(100000));
        System.out.println(nested());
        int[] c = grid(100000);
        System.out.println(c[0] + " " + c[1] + " " + c[2] + " " + c[3]);
        String carry = "";
        for (int round = 0; round < 3; round++) {
            for (int i = 0; i < 1000; i++) {
                String s = "x" + i;
                if (s.length() == 4 && i % 7 == 0) { carry = carry.length() + ":" + s; }   // outer variable: not a region
            }
        }
        System.out.println(carry);
    }
}
