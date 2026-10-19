// `continue` out of a block that declared a local, in a loop body that declares none of its own: the local stayed on
// the ashvm stack (the loop's cleanup only cut off what the body itself declared), and every local after the loop
// landed in the wrong slot.
public class run_next_locals_j {
    static long h = 55865;
    static int[] ar = {3, 1, 4};

    static long nested() {
        int[] a1 = new int[2];
        a1[0] = 2;
        for (int i6 = 0; i6 < 3; i6++) {
            for (int i7 = 0; i7 < 4; i7++) {
                if (h % 2 == 0) {
                    int w8 = 0;
                    while (w8 < 3) {
                        w8++;
                    }
                    if ((3 + i7) % 2 == 0) {
                        h = (h * 31 + a1[0] + a1.length) % 1000003L;
                        continue;
                    }
                }
                for (int i9 = 0; i9 < 4; i9++) {
                    h = (h * 31 + 5 + i7) % 1000003L;
                    for (int x10 : ar) {
                        h = (h * 31 + x10) % 1000003L;
                    }
                }
            }
            for (int x11 : ar) {
                h = (h * 31 + x11 + a1[0] + a1.length) % 1000003L;
            }
        }
        int[] after = new int[3];
        after[0] = 7;
        return h + after[0] + after.length;
    }

    static int whileForm() {
        int n = 0, i = 0;
        while (i < 6) {
            i++;
            if (i % 2 == 0) {
                int t = i * 10;
                String s = "s" + t;
                if (s.length() > 2) continue;
                n += t;
            }
            n += 1;
        }
        int[] tail = {n, i};
        return tail[0] * 100 + tail[1];
    }

    public static void main(String[] args) {
        System.out.println(nested());
        System.out.println(whileForm());
    }
}
