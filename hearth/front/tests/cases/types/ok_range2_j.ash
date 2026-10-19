package t;

public class OkRange2 {
    private int[] data;
    private int[] mixed = new int[4];
    private int[] open;
    static int[] shared = new int[3];

    OkRange2(int n) { data = new int[n]; open = new int[2]; }
    void grow() { mixed = new int[16]; }                  // assigned a bigger array too: 12 is fine somewhere
    void take(int[] x) { open = x; }                      // an unknown array: no length is claimed

    int fine() { return mixed[10] + open[5] + data[100]; }

    static char chars(String param, int n) {
        String s = "abc";
        char c = s.charAt(2);
        s += param;                                       // unknown length now
        c += s.charAt(7);
        String t = "ab";
        for (int i = 0; i < 3; i++) t += "c";             // grows in a loop: unknown
        c += t.charAt(4);
        String u = n > 0 ? "a" : "abcdefgh";              // not a plain constant
        c += u.charAt(6);
        String v = "é";                                   // not ASCII: the length is left unknown
        c += v.charAt(1);
        try { c += "abc".charAt(9); } catch (Exception e) { c += 1; }
        return c;
    }

    static int cases(int x, int[] p) {
        int[] a = new int[4];
        int s = 0;
        switch (x) {
            case 1: s += a[x]; break;
            case 2:
            case 3: s += a[x]; break;                     // 2 or 3
            case 9: if (x == 9) s += 1; break;
            default: s += p[x];
        }
        switch (x) {
            case 7: s += 1;                               // falls through: x may be 7 or 8 below
            case 8: s += a[x - 6]; break;
            default: break;
        }
        return s;
    }

    static int arrow(int x) {
        int[] a = new int[3];
        int y = switch (x) {
            case 0 -> a[x];
            case 1, 2 -> a[x];
            default -> 0;
        };
        return y;
    }

    static class Cell {
        int[] arr = new int[2];
    }

    static class Writer {
        void widen(Cell c) { c.arr = new int[20]; }       // another object of the class gets a bigger array
        int read(Cell c) { return c.arr[10]; }
    }
}
