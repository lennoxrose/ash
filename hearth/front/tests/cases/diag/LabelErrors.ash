package t;

public class Main {
    static int missing(int n) {
        outer:
        while (true) {
            for (;;) {
                if (n > 3) break outer;      // the labeled loop can complete, so the method needs a return after it
                n++;
            }
        }
    }

    static int fine(int n) {
        outer:
        while (true) {
            for (;;) {
                if (n > 3) return n;
                continue outer;
            }
        }
    }

    static void unreachable(int n) {
        outer:
        for (int i = 0; i < n; i++) {
            for (;;) {
                if (i > 2) break outer;
                continue outer;
            }
            n++;                              // the inner loop never completes: it leaves by a labeled jump
        }
    }

    static void afterJump(int n) {
        outer:
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                break outer;
                n++;
            }
        }
    }

    static void unknown(int n) {
        for (int i = 0; i < n; i++) {
            break nowhere;
        }
        for (int i = 0; i < n; i++) {
            continue other;
        }
    }

    static void duplicate(int n) {
        a:
        for (int i = 0; i < n; i++) {
            a:
            for (int j = 0; j < n; j++) {
                break a;
            }
        }
    }

    static void lambdas(int n) {
        outer:
        for (int i = 0; i < n; i++) {
            Runnable r = () -> {
                break outer;
            };
            Runnable q = () -> {
                for (int k = 0; k < 3; k++) {
                    continue outer;
                }
            };
            Runnable t = () -> {
                outer:                        // like javac: a label cannot be reused inside the scope of the same label
                for (int k = 0; k < 3; k++) {
                    break outer;
                }
            };
        }
    }

    static void afterLoop(int n) {
        outer:
        for (int i = 0; i < n; i++) {
            n++;
        }
        break outer;                          // the loop is over: its label is out of scope
    }

    static void sibling(int n) {
        a:
        for (int i = 0; i < n; i++) {
            n++;
        }
        for (int i = 0; i < n; i++) {
            continue a;
        }
    }

    public static void main(String[] args) {
        missing(1);
    }
}
