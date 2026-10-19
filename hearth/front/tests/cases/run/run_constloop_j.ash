package t;

// Loops with a constant true condition that are left by `return`: no statement is needed after them.
public class RunConstLoop {
    static final bool T = true;

    static int first(int[] xs, int want) {
        int i = 0;
        while (1 < 2) {
            if (xs[i] == want) return i;
            i++;
        }
    }

    static int countDown(int n) {
        final bool go = true;
        while (go) {
            if (n <= 0) return 100;
            n -= 3;
        }
    }

    static int steps(int n) {
        int k = 0;
        for (; !false && T; ) {
            k++;
            if (k * k > n) return k;
        }
    }

    public static void main(String[] args) {
        int[] xs = {4, 8, 15, 16, 23, 42};
        System.out.println(first(xs, 16) + " " + first(xs, 4) + " " + first(xs, 42));
        System.out.println(countDown(10));
        System.out.println(steps(50));
        int i = 0;
        do {
            i++;
            if (i == 5) break;
        } while (T);
        System.out.println(i);
    }
}
