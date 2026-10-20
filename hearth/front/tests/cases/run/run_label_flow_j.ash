import ash.util.List;
import ash.util.ArrayList;

// Programs that are valid and must be accepted: the move, range and initialisation checks follow the states that
// leave a loop through a labeled break or continue (a rejected program here would be a false positive).
public class run_label_flow_j {
    static int[] keep;
    static void hold(int[] p) { keep = p; }

    static int indexTwoOrFive() {
        int[] a = new int[4];
        a[2] = 9;
        int i = 5;
        outer:
        for (int k = 0; k < 3; k++) {
            for (int j = 0; j < 3; j++) {
                if (k + j == 2) { i = 2; break outer; }
            }
        }
        return a[i];
    }

    static int overflowOnlyOnOnePath() {
        int x = 2000000000;
        outer:
        for (int k = 0; k < 3; k++) {
            for (int j = 0; j < 3; j++) {
                x = 1;
                break outer;
            }
        }
        return x + x;
    }

    static int divisorOnlyZeroOnOnePath() {
        int d = 0;
        outer:
        for (int k = 0; k < 3; k++) {
            for (int j = 0; j < 3; j++) {
                if (j == 1) { d = 5; continue outer; }
            }
            d = 4;
        }
        return 100 / (d + 1);
    }

    static int movedOnOtherPathOnly() {
        int[] a = {1, 2, 3};
        int[] b = {4};
        outer:
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (j == 1) {
                    hold(b);
                    break outer;
                }
            }
        }
        return a[0] + a.length;
    }

    static int reassignedAfterMove() {
        int[] a = {1, 2, 3};
        outer:
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (j == 1) {
                    hold(a);
                    a = new int[]{i, j};          // a lives again before the jump
                    continue outer;
                }
            }
        }
        return a[0] + a.length + keep.length;
    }

    static int movedInDeclaredInsideLoop() {
        int total = 0;
        outer:
        for (int i = 0; i < 4; i++) {
            int[] a = {i, i + 1};
            for (int j = 0; j < 4; j++) {
                if (j == 2) {
                    hold(a);                      // a is a new variable on every pass of the outer loop
                    continue outer;
                }
                total += a[0] + j;
            }
        }
        return total + keep[0];
    }

    static int assignedOnLabeledPath() {
        int x;
        outer:
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (j == 1) { x = 1; break outer; }
            }
        }
        int y;
        y = 2;
        return y;
    }

    static int sameLabelTwice() {
        int n = 0;
        outer:
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) { if (j == 1) continue outer; n++; }
        }
        outer:
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) { if (i == 1) break outer; n += 10; }
        }
        return n;
    }

    static int lambdaWithItsOwnLabel() {
        int[] count = new int[1];
        outer:
        for (int i = 0; i < 2; i++) {
            Runnable r = () -> {
                inl:
                for (int a = 0; a < 3; a++) {
                    for (int b = 0; b < 3; b++) {
                        if (b == 1) continue inl;
                        count[0] += 1;
                    }
                }
            };
            r.run();
            if (i == 0) continue outer;
            break outer;
        }
        return count[0];
    }

    static int listSizes() {
        List<Integer> l = new ArrayList<>();
        outer:
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (j == 2) continue outer;
                l.add(i * 10 + j);
            }
        }
        return l.size() + l.get(3);
    }

    static int dead() {
        int n = 0;
        outer:
        while (true) {
            n++;
            inner:
            while (true) {
                n += 2;
                if (n > 20) break outer;
                if (n % 3 == 0) continue outer;
                if (n % 5 == 0) break inner;
            }
            n += 100;
            if (n > 500) break;
        }
        return n;
    }

    public static void main(String[] args) {
        System.out.println(indexTwoOrFive());
        System.out.println(overflowOnlyOnOnePath());
        System.out.println(divisorOnlyZeroOnOnePath());
        System.out.println(movedOnOtherPathOnly());
        System.out.println(reassignedAfterMove());
        System.out.println(movedInDeclaredInsideLoop());
        System.out.println(assignedOnLabeledPath());
        System.out.println(sameLabelTwice());
        System.out.println(lambdaWithItsOwnLabel());
        System.out.println(listSizes());
        System.out.println(dead());
    }
}
