// Loops that call project methods which provably keep nothing they allocate (static, private or final; only
// primitives and Strings in and a primitive out; every call inside them of the same kind; no recursion): the
// callee's garbage dies with the iteration (jown/region.c). Every loop below must be a region.
import ash.util.ArrayList;
import ash.util.List;

public class RegionCalls {
    static int calls;
    static String[] table = {"zero", "one", "two", "three"};
    private int base = 7;
    private int[] data = {4, 5, 6};

    static int width(String s) {
        String padded = "[" + s + "]";
        return padded.length();
    }

    static int digits(int n) {
        String s = "" + n;
        return width(s) - 2;
    }

    static long mix(int a, long b) {
        int[] tmp = new int[6];
        for (int i = 0; i < tmp.length; i++) tmp[i] = a * i;
        String label = "m" + a + ":" + b;
        calls++;
        return tmp[5] + label.length() + b;
    }

    static bool even(int n) {
        String t = "e" + n;
        return n % 2 == 0 && t.length() > 1;
    }

    static double scale(double x, String name) {
        String key = name + x;
        return x * key.length();
    }

    static char first(String s) {
        String copy = s + s;
        return copy.charAt(0);
    }

    static int lookup(int i) {
        String name = table[i % table.length];
        String loud = name + "!";
        return loud.length();
    }

    static int tableWords(int rounds) {
        int total = 0;
        for (int r = 0; r < rounds; r++) {
            String[] words = new String[3];
            for (int w = 0; w < 3; w++) words[w] = "w" + r + w;
            total += words[0].length() + words[2].length();
        }
        return total;
    }

    private int plus(int x) {
        String s = "p" + (x + base);
        return s.length() + data[1];
    }

    final int times(int x) {
        String s = "t" + x * base;
        return plus(x) + s.length();
    }

    static long viaStatic(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            total += digits(i * 37) + width("abc" + i);
        }
        return total;
    }

    static long viaMix(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            total += mix(i, 100000L * i);
            if (even(i)) total += 1;
        }
        return total + calls;
    }

    static double viaDouble(int n) {
        double total = 0;
        for (int i = 0; i < n; i++) {
            total += scale(i + 0.5, "k" + i);
            total += first("q" + i);
        }
        return total;
    }

    static long viaForEach() {
        long total = 0;
        for (String w : table) total += width(w) + lookup(w.length());
        return total;
    }

    static long viaCallOnly(int n) {
        // nothing is allocated in the loop itself: only the callee allocates
        long total = 0;
        for (int i = 0; i < n; i++) total += lookup(i) + tableWords(2);
        return total;
    }

    long viaInstance(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            total += plus(i) + times(i);
            data[0] += 1;
        }
        return total;
    }

    static long viaObject(RegionCalls rc, int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            String tag = "o" + i;
            total += rc.times(i) + tag.length();
        }
        return total;
    }

    static long nested(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < 3; j++) total += digits(i * j) + lookup(j);
        }
        return total;
    }

    static long afterwards(int n) {
        long total = viaStatic(n);
        String keep = "keep-" + total;
        int[] more = new int[3];
        for (int i = 0; i < n; i++) more[i % 3] += digits(i) + keep.length();
        return total + keep.length() + more[0] + more[1] + more[2];
    }

    public static void main(String[] args) {
        RegionCalls rc = new RegionCalls();
        System.out.println(viaStatic(40));
        System.out.println(viaMix(30));
        System.out.println(viaDouble(12));
        System.out.println(viaForEach());
        System.out.println(viaCallOnly(10));
        System.out.println(rc.viaInstance(9));
        System.out.println(viaObject(rc, 9));
        System.out.println(nested(7));
        System.out.println(afterwards(25));
    }
}
