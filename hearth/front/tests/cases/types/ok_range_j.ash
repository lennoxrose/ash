package t;

public class OkRange {
    static int safe(int[] p, int n) {
        int[] a = new int[5];
        int s = 0;
        for (int i = 0; i < a.length; i++) s += a[i];
        for (int i = 0; i < 5; i++) { a[i] = i; s += a[i]; }
        int j = 4;
        while (j >= 0) { s += a[j]; j--; }
        int k = 0;
        if (k < 3) s += a[k + 4];
        int big = 2000000000;
        if (big < 0) { int w = big + big; s += w; }     // cannot run: big is positive
        int d = 0;
        if (d != 0) s += 10 / d;                        // cannot run
        long l = 2000000000L;
        l = l + l;
        int m = n;
        s += m + m;                                     // unknown n: may fit
        try { s += a[9]; } catch (Exception e) { s += 1; }
        try { s += 10 / d; } catch (Exception e) { s += 2; }
        int t = 0;
        for (int q = 0; q < 100; q++) { t += q; if (t > 50) break; }
        s += t;
        int idx = 0;
        do { s += a[idx]; idx++; } while (idx < 5);
        return s + p[n];
    }

    static int rec(int n) {
        if (n == 0) return 0;
        return rec(n - 1) + 1;
    }
}
