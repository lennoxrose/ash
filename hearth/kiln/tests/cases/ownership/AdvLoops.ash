// Ranges through nested loops, labeled break / continue, and counters changed in odd places. Every access is
// valid; the analysis must not claim otherwise.
public class AdvLoops {
    static int f(int n) { return n * 2 + 1; }

    public static void main(String[] args) {
        int[] a = new int[5];
        int s = 0;

        for (int i = 0; i < 10; i++) { if (i >= 5) break; a[i] = i; }

        int k = 0;
        while (true) { if (k == 5) break; a[k++] += 1; }

        for (int i = 0; i <= 4; i++) for (int j = i; j < 5; j++) a[j] += i;

        int idx = 7; idx -= 3; idx %= 4; a[idx] = 2;

        int m = 3; while (m < 5) { m += 1; } a[m - 1] += 3;

        for (int i = 0; i < 5; i += 2) a[i] += 10;
        for (int i = 4; i >= 0; i--) a[i] += 1;

        bool done = false;
        for (int i = 0; i < 5 && !done; i++) {
            for (int j = 0; j < 5; j++) {
                if (j > i) break;
                if (i + j > 6) { done = true; break; }
                a[j] += i;
            }
        }

        int p = 0;
        do { a[p] += 1; p += 2; } while (p < 5);

        int lo = 0, hi = 4;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (a[mid] % 2 == 0) lo = mid + 1; else hi = mid;
            a[lo] += 0;
        }

        int q = 1;
        for (int i = 0; i < 3; i++) { q = q * 2; }         // 8
        if (q < 5) a[q] = 1;                                // dead: q is 8
        if (q > 7) a[q - 4] += 1;                           // a[4]

        long big = 1;
        for (int i = 0; i < 40; i++) big *= 2;               // long: no overflow of int
        s += (int) (big % 1000);

        int w = 0;
        for (int i = 0; i < 5; i++) {
            int t = f(i);
            if (t > 100) w = 99;       // never
            a[w + (i % 2)] += 1;
        }

        int c = 0;
        for (int i = 0; i < 5; i++) { c = i; }
        a[c] += 1;                      // c is 4 after a loop that ran 5 times

        int e = 0;
        for (int i = 0; i < 5; i++) { if (i == 3) { e = 4; } }
        a[e] += 1;                      // e is 0 or 4

        for (int v : a) s += v;
        System.out.println(s);
        StringBuilder sb = new StringBuilder();
        for (int v : a) sb.append(v).append(' ');
        System.out.println(sb.toString().trim());
    }
}
