import ash.util.List;
import ash.util.ArrayList;

// Labeled jumps over sole-owner locals (fresh Strings, arrays, objects, lists the compiler frees at the end of their
// block): every path out of the loops frees each of them exactly once, and nothing is freed that a path still uses.
public class run_label_own_j {
    static class Box {
        int[] d;
        String n;
        Box(String n, int k) { this.n = n; d = new int[k]; d[0] = k; }
        int size() { return n.length() + d[0]; }
    }

    static String kept;
    static int[] keptArr;
    static void keep(String s) { kept = s; }
    static void keepA(int[] a) { keptArr = a; }

    static int outerLocals() {
        int total = 0;
        outer:
        for (int i = 0; i < 6; i++) {
            String a = "outer" + i;
            int[] arr = new int[i + 1];
            arr[0] = i;
            for (int j = 0; j < 6; j++) {
                String b = a + "-" + j;
                Box bx = new Box("n" + j, j + 1);
                List<String> l = new ArrayList<>();
                l.add(b);
                if (j == 2 && i % 2 == 0) continue outer;
                if (i == 4) break outer;
                total += b.length() + bx.size() + l.size() + arr[0];
            }
            total += a.length();
        }
        return total;
    }

    static int movedThenJump() {
        int total = 0;
        outer:
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                String s = "m" + i + j;
                int[] a = new int[3];
                a[1] = j;
                if (j == 1) {
                    keep(s);                 // s moves; a is still freed on the jump
                    continue outer;
                }
                if (i == 3) {
                    keepA(a);                // a moves; s is freed
                    break outer;
                }
                total += s.length() + a[1];
            }
        }
        return total + kept.length() + keptArr.length;
    }

    static int reassigned() {
        int total = 0;
        String acc = "x" + total;
        outer:
        for (int i = 0; i < 4; i++) {
            String t = "t" + i;
            for (int j = 0; j < 4; j++) {
                String u = t + j;
                acc = acc + u;
                if (acc.length() > 20) {
                    total += acc.length();
                    acc = "r" + i;
                    continue outer;
                }
                if (i == 3) break outer;
            }
        }
        return total + acc.length();
    }

    static int viaSwitchAndTry() {
        int total = 0;
        outer:
        for (int i = 0; i < 5; i++) {
            String s = "s" + i;
            switch (i % 3) {
                case 0:
                    for (int j = 0; j < 3; j++) {
                        String q = s + j;
                        if (j == 1) continue outer;
                        total += q.length();
                    }
                    break;
                case 1:
                    try {
                        String z = "z" + s;
                        if (i == 4) break outer;
                        total += z.length();
                    } finally {
                        total += 1000;
                    }
                    break;
                default:
                    total += s.length();
            }
            total += s.length();
        }
        return total;
    }

    static int whileObjects() {
        int total = 0, i = 0;
        outer:
        while (i < 6) {
            i++;
            Box b = new Box("w" + i, i);
            int k = 0;
            do {
                String s = b.n + k;
                int[] a = new int[2];
                a[0] = k;
                k++;
                if (k == 2 && i % 2 == 0) continue outer;
                if (i == 5) break outer;
                total += s.length() + a[0] + b.size();
            } while (k < 4);
            total += b.size();
        }
        return total;
    }

    public static void main(String[] args) {
        System.out.println(outerLocals());
        System.out.println(movedThenJump());
        System.out.println(reassigned());
        System.out.println(viaSwitchAndTry());
        System.out.println(whileObjects());
    }
}
