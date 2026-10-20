package t;

public class Main {
    static int[] keep;
    static void hold(int[] p) { keep = p; }

    static void breakOut(int n) {
        int[] a = {1, 2, 3};
        outer:
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (j == 1) {
                    hold(a);                  // a moves on the way out
                    break outer;
                }
            }
        }
        System.out.println(a[0]);             // E0101: moved on the labeled path
    }

    static void continueOut(int n) {
        int[] a = {1, 2, 3};
        outer:
        for (int i = 0; i < n; i++) {
            System.out.println(a[0]);         // E0101 on the second pass: the labeled continue below moved a
            for (int j = 0; j < n; j++) {
                if (j == 1) {
                    hold(a);
                    continue outer;
                }
            }
        }
    }

    static void viaFinally(int n) {
        int[] a = {1, 2, 3};
        outer:
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                try {
                    if (j == 1) break outer;
                } finally {
                    hold(a);                  // the finally of the try being left runs on the jump
                }
            }
        }
        System.out.println(a[0]);             // E0101
    }

    static void notMoved(int n) {
        int[] a = {1, 2, 3};
        int[] b = {4};
        outer:
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (j == 1) {
                    hold(b);
                    break outer;
                }
            }
        }
        System.out.println(a[0]);             // fine: only b moved
    }

    static void uninit(int n) {
        int x;
        outer:
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (j == 1) { x = 1; break outer; }
            }
        }
        int y;
        for (int i = 0; i < n; i++) {
            y = i;
        }
        System.out.println(x);                // fine: the labeled path assigned it, and the other paths might have
    }

    static void ranges() {
        int[] a = new int[4];
        int i = 5;
        outer:
        for (int k = 0; k < 3; k++) {
            for (int j = 0; j < 3; j++) {
                if (k + j == 2) { i = 2; break outer; }
            }
        }
        System.out.println(a[i]);             // fine: i is 2 or 5 here
        int m = 4;
        done:
        for (int k = 0; k < 3; k++) {
            for (int j = 0; j < 3; j++) {
                m = 7;
                continue done;
            }
        }
        System.out.println(a[m]);             // E0202: m is 4 or 7 on every path, both past the end
    }

    public static void main(String[] args) {
        breakOut(3);
    }
}
