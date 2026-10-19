// A lambda written inside a loop has loops (and break / continue) of its own: the jumps of the loop around it were
// recorded in the slots the lambda's loops then reused, so the outer `continue` / `break` were patched with the
// lambda's offsets (a corrupted chunk on ashvm, a crash on kiln).
public class run_lambda_loops_j {
    static int viaLambda() {
        int[] count = new int[1];
        for (int i = 0; i < 4; i++) {
            Runnable r = () -> {
                for (int a = 0; a < 3; a++) {
                    for (int b = 0; b < 3; b++) {
                        if (b == 1) continue;
                        if (a == 2) break;
                        count[0] += 1;
                    }
                    if (a == 1) continue;
                    count[0] += 10;
                }
            };
            r.run();
            if (i == 0) continue;
            if (i == 2) break;
            count[0] += 100;
        }
        return count[0];
    }

    static int nested() {
        int[] count = new int[1];
        int n = 0;
        while (n < 5) {
            n++;
            if (n == 2) continue;
            Runnable outer = () -> {
                for (int a = 0; a < 3; a++) {
                    Runnable inner = () -> {
                        int k = 0;
                        while (true) {
                            k++;
                            if (k == 2) continue;
                            if (k > 3) break;
                            count[0] += k;
                        }
                    };
                    inner.run();
                    if (a == 1) continue;
                    count[0] += 1000;
                }
            };
            outer.run();
            if (n == 4) break;
        }
        return count[0] * 10 + n;
    }

    public static void main(String[] args) {
        System.out.println(viaLambda());
        System.out.println(nested());
    }
}
