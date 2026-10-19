// Labeled break / continue and finally: every finally between the jump and the labeled loop runs, innermost first,
// and the ones outside the labeled loop do not (until the loop is left normally).
public class run_label_finally_j {
    static String log = "";
    static int count;

    static void fin1() {
        outer:
        for (int i = 0; i < 3; i++) {
            try {
                log += "[t" + i;
                for (int j = 0; j < 3; j++) {
                    try {
                        try {
                            if (j == 1) continue outer;
                            log += "a" + j;
                        } finally {
                            log += "(f1)";
                        }
                    } finally {
                        log += "(f2)";
                    }
                }
            } finally {
                log += "(F" + i + ")]";
            }
        }
        System.out.println("fin1 " + log);
        log = "";
    }

    static void fin2() {
        try {
            outer:
            while (true) {
                try {
                    do {
                        count++;
                        if (count == 4) break outer;
                        if (count % 2 == 0) continue outer;
                        log += "d" + count;
                    } while (true);
                } finally {
                    log += "|in" + count;
                }
            }
            log += "|after";
        } finally {
            log += "|out";
        }
        System.out.println("fin2 " + log);
        log = "";
    }

    static int fin3() {
        int x = 0;
        outer:
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                try {
                    if (i == 1) continue outer;
                    if (i == 3) break outer;
                    x += 1;
                } finally {
                    x += 100;
                    if (j == 2) log += "j2;";
                }
            }
        }
        System.out.println("fin3 " + x + " " + log);
        log = "";
        return x;
    }

    // a finally that has loops and jumps of its own
    static void fin4() {
        outer:
        for (int i = 0; i < 3; i++) {
            inner:
            for (int j = 0; j < 3; j++) {
                try {
                    if (j == 1) continue outer;
                    log += "b" + i + j;
                } finally {
                    for (int k = 0; k < 4; k++) {
                        if (k == 1) continue;
                        if (k == 3) break;
                        log += "k" + k;
                    }
                }
            }
        }
        System.out.println("fin4 " + log);
        log = "";
    }

    // the catch clause jumps; the finally still runs
    static void fin5() {
        outer:
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                try {
                    if (j == 1) throw new RuntimeException("x" + i);
                    log += "ok" + i + j + ";";
                } catch (RuntimeException e) {
                    log += e.getMessage() + ";";
                    if (i == 2) break outer;
                    continue outer;
                } finally {
                    log += "f;";
                }
            }
        }
        System.out.println("fin5 " + log);
        log = "";
    }

    // an exception thrown by a finally replaces the jump
    static void fin6() {
        try {
            outer:
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    try {
                        if (j == 1) break outer;
                    } finally {
                        if (i == 0 && j == 1) throw new IllegalStateException("from finally");
                    }
                }
            }
            log += "not reached";
        } catch (IllegalStateException e) {
            log += "caught " + e.getMessage();
        }
        System.out.println("fin6 " + log);
        log = "";
    }

    // a labeled jump out of a lambda-free helper loop in a method that returns inside a finally
    static int fin7() {
        outer:
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                try {
                    if (j == 1) continue outer;
                } finally {
                    count += 10;
                }
            }
        }
        return count;
    }

    public static void main(String[] args) {
        fin1();
        count = 0;
        fin2();
        fin3();
        fin4();
        fin5();
        fin6();
        count = 0;
        System.out.println("fin7 " + fin7());
    }
}
