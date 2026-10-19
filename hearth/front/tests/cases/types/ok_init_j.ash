package t;

public class OkInit {
    static void fill(int[] out, int v) { out[0] = v; }

    static int maybe(bool f, int n) {
        int a;
        if (f) a = 1;                                    // only some paths: the run-time value is checked, not here
        int s = 0;
        if (f) s += a;
        int b;
        if (f) b = 1; else b = 2;
        s += b;
        int c;
        switch (n) {
            case 1: c = 10; break;
            case 2: c = 20; break;
            default: c = 0;
        }
        s += c;
        int d;
        try { d = 10 / (n + 1); } catch (Exception e) { d = 0; }
        s += d;
        int e;
        while (true) { e = n; if (n > 0) break; n++; }
        s += e;
        int g;
        for (int i = 0; i < 3; i++) {
            int h;
            if (i == 0) h = 1; else h = i;
            g = h;
            s += g;
        }
        int k;
        do { k = n; } while (k < 0);
        s += k;
        int[] arr = new int[1];
        int m;
        m = arr[0] = 4;
        s += m;
        int first;
        bool seen = false;
        for (int i = 0; i < 3; i++) {
            if (!seen) { first = i; seen = true; } else { s += i; }
        }
        int late;
        Runnable r = () -> System.out.println("later");
        late = 3;
        r.run();
        s += late;
        int z;
        if (f && (z = n) > 0) s += z;
        String t;
        t = f ? "yes" : "no";
        s += t.length();
        return s;
    }
}
