// Sixteen nested loops of three kinds: the flow analysis walks a nest again for every pass of the loop around it, so
// this once took half a minute; it must check in a moment (and clean).
package t;

public class OkDeepNest {
    static int[] kept;
    static void keep(int[] p) { kept = p; }
    static void run() {
        int[] a = new int[5];
        int[] b = new int[5];
        int s = 0;
        for (int i0 = 0; i0 < 3; i0++) {
            if (s > 0) { s -= 0; } else { s += 1; }
            a[0] = s;
            int i1 = 0;
            while (i1 < 3) {
                if (s > 1) { s -= 1; } else { s += 2; }
                a[1] = s;
                for (int q2 : a) {
                    if (s > 2) { s -= 2; } else { s += 3; }
                    a[2] = s;
                    for (int i3 = 0; i3 < 3; i3++) {
                        if (s > 3) { s -= 3; } else { s += 4; }
                        a[3] = s;
                        int i4 = 0;
                        while (i4 < 3) {
                            if (s > 4) { s -= 4; } else { s += 5; }
                            a[4] = s;
                            for (int q5 : a) {
                                if (s > 5) { s -= 5; } else { s += 6; }
                                a[0] = s;
                                for (int i6 = 0; i6 < 3; i6++) {
                                    if (s > 6) { s -= 6; } else { s += 7; }
                                    a[1] = s;
                                    int i7 = 0;
                                    while (i7 < 3) {
                                        if (s > 7) { s -= 7; } else { s += 8; }
                                        a[2] = s;
                                        for (int q8 : a) {
                                            if (s > 8) { s -= 8; } else { s += 9; }
                                            a[3] = s;
                                            for (int i9 = 0; i9 < 3; i9++) {
                                                if (s > 9) { s -= 9; } else { s += 10; }
                                                a[4] = s;
                                                int i10 = 0;
                                                while (i10 < 3) {
                                                    if (s > 10) { s -= 10; } else { s += 11; }
                                                    a[0] = s;
                                                    for (int q11 : a) {
                                                        if (s > 11) { s -= 11; } else { s += 12; }
                                                        a[1] = s;
                                                        for (int i12 = 0; i12 < 3; i12++) {
                                                            if (s > 12) { s -= 12; } else { s += 13; }
                                                            a[2] = s;
                                                            int i13 = 0;
                                                            while (i13 < 3) {
                                                                if (s > 13) { s -= 13; } else { s += 14; }
                                                                a[3] = s;
                                                                for (int q14 : a) {
                                                                    if (s > 14) { s -= 14; } else { s += 15; }
                                                                    a[4] = s;
                                                                    for (int i15 = 0; i15 < 3; i15++) {
                                                                        if (s > 15) { s -= 15; } else { s += 16; }
                                                                        a[0] = s;
                                                                        s += a[2];
                                                                    }
                                                                }
                                                                i13++;
                                                            }
                                                        }
                                                    }
                                                    i10++;
                                                }
                                            }
                                        }
                                        i7++;
                                    }
                                }
                            }
                            i4++;
                        }
                    }
                }
                i1++;
            }
        }
        System.out.println(s);
    }
    public static void main(String[] args) { run(); }
}
