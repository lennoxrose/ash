// Labeled break / continue and loop regions: a jump that leaves a region loop (from an inner loop, or from the loop
// itself) gives the iteration's memory back at the jump, nothing it took is read afterwards, and the memory
// around the loops (values made before them, and after) stays intact. With KILN_REGION_POISON the reclaimed bytes
// are overwritten, so a value that outlived its iteration shows up as garbage.
public class RegionLabels {
    static int width(String s) { String t = s + "!"; return t.length(); }

    // the labeled loop is a region and the jump comes from an inner region
    static long breakOuter() {
        long total = 0;
        String before = "before-" + 7;
        outer:
        for (int i = 0; i < 400; i++) {
            String a = "outer-" + i;
            for (int j = 0; j < 400; j++) {
                String b = a + "/" + j;
                int[] cells = new int[8];
                cells[j % 8] = b.length();
                total += cells[j % 8];
                if (i == 3 && j == 5) break outer;
            }
        }
        String after = "after-" + total;
        return total + before.length() + after.length();
    }

    // `continue outer` from the inner region on every iteration: the memory must not pile up (see region_check.sh)
    static long continueOuter() {
        long total = 0;
        outer:
        for (int i = 0; i < 20000; i++) {
            String a = "row-" + i + "-" + (i * 31);
            for (int j = 0; j < 20; j++) {
                String b = a + "/" + j;
                int[] cells = new int[16];
                cells[j % 16] = b.length();
                if (j == 2) continue outer;
                total += cells[j % 16];
            }
            total += 1000000;
        }
        return total;
    }

    // the labeled loop is not a region (it keeps a value in an outer variable); the inner one is
    static long outerKeeps() {
        long total = 0;
        String carry = "start";
        outer:
        for (int i = 0; i < 300; i++) {
            carry = carry.length() > 40 ? "s" + i : carry + i;          // a reference stored in an outer variable
            for (int j = 0; j < 300; j++) {
                String t = "t" + j + "." + i;
                if (t.length() > 6) continue outer;
                total += t.length();
            }
        }
        return total + carry.length();
    }

    // three levels: a jump from the innermost to the outermost passes two regions
    static long deep() {
        long total = 0;
        a:
        for (int i = 0; i < 100; i++) {
            String s1 = "a" + i;
            b:
            for (int j = 0; j < 100; j++) {
                String s2 = s1 + "b" + j;
                for (int k = 0; k < 100; k++) {
                    String s3 = s2 + "c" + k;
                    total += s3.length();
                    if (k == 3 && j % 2 == 0) continue b;
                    if (k == 5) continue a;
                    if (i == 50) break a;
                }
            }
        }
        return total;
    }

    // while and do-while regions, a pure callee, and a break out of the labeled loop after the inner one allocated
    static long whiles() {
        long total = 0;
        int i = 0;
        outer:
        while (i < 5000) {
            i++;
            String s = "w" + i;
            int k = 0;
            do {
                String u = s + k;
                total += width(u);
                k++;
                if (k == 2 && i % 3 == 0) continue outer;
                if (i == 4000) break outer;
            } while (k < 6);
        }
        return total;
    }

    // for-each over arrays
    static long foreach() {
        int[] xs = {1, 2, 3, 4, 5};
        int[] ys = {10, 20, 30};
        long total = 0;
        outer:
        for (int round = 0; round < 3000; round++) {
            for (int x : xs) {
                String s = "x" + x + "r" + round;
                for (int y : ys) {
                    String t = s + y;
                    if (y == 20 && x % 2 == 0) continue outer;
                    if (round == 2500 && x == 4) break outer;
                    total += t.length();
                }
            }
        }
        return total;
    }

    // a try with a finally inside the region: the finally runs before the rewind
    static long withFinally() {
        long total = 0;
        outer:
        for (int i = 0; i < 2000; i++) {
            String a = "f" + i;
            for (int j = 0; j < 10; j++) {
                String b = a + j;
                try {
                    if (j == 3) continue outer;
                    total += b.length();
                } finally {
                    total += a.length() + b.length();
                }
            }
        }
        return total;
    }

    public static void main(String[] args) {
        System.out.println(breakOuter());
        System.out.println(continueOuter());
        System.out.println(outerKeeps());
        System.out.println(deep());
        System.out.println(whiles());
        System.out.println(foreach());
        System.out.println(withFinally());
        String tail = "";
        for (int i = 0; i < 5; i++) tail += "z" + i;
        System.out.println(tail);
    }
}
