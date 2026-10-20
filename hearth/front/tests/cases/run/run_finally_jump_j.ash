// `break` and `continue` run the finally blocks of the try statements they leave, innermost first, before the jump
// (they were skipped; `return` always ran them).
public class run_finally_jump_j {
    static int k(int v) { return v; }

    static void breakOut() {
        int x = 0;
        while (true) {
            try {
                break;
            } finally {
                x = 5;
            }
        }
        System.out.println("break: x = " + x);
    }

    static void continueLoop() {
        int y = 0;
        int seen = 0;
        for (int i = 0; i < 3; i++) {
            try {
                if (i == 1) continue;
                seen++;
            } finally {
                y += 10;
            }
        }
        System.out.println("continue: y = " + y + ", seen = " + seen);
    }

    static void nested() {
        String log = "";
        for (int i = 0; i < 2; i++) {
            try {
                try {
                    if (i == 0) continue;
                    log += "body" + i + ";";
                    break;
                } finally {
                    log += "inner" + i + ";";
                }
            } finally {
                log += "outer" + i + ";";
            }
        }
        System.out.println("nested: " + log);
    }

    static void onlyTheTriesInsideTheLoop() {
        String log = "";
        try {
            for (int i = 0; i < 3; i++) {
                try {
                    if (i == 1) break;
                    log += "t" + i + ";";
                } finally {
                    log += "f" + i + ";";
                }
            }
            log += "after;";
        } finally {
            log += "outer;";
        }
        System.out.println("scope: " + log);
    }

    static void inCatch() {
        int n = 0;
        for (int i = 0; i < 3; i++) {
            try {
                if (i == 1) throw new RuntimeException("x");
                n += 1;
            } catch (RuntimeException e) {
                n += 100;
                continue;
            } finally {
                n += 10;
            }
        }
        System.out.println("catch: n = " + n);
    }

    static void inSwitch() {
        int count = 0;
        for (int i = 0; i < 4; i++) {
            try {
                switch (i) {
                    case 1:
                        continue;
                    case 2:
                        count += 1;
                        break;
                    default:
                        count += 2;
                }
                count += 1000;
            } finally {
                count += 10;
            }
        }
        System.out.println("switch: count = " + count);
    }

    static void doLoop() {
        int z = 0;
        int i = 0;
        do {
            i++;
            try {
                if (i == 2) continue;
                z += 1;
            } finally {
                z += 100;
            }
        } while (i < 3);
        System.out.println("do: z = " + z + ", i = " + i);
    }

    public static void main(String[] args) {
        breakOut();
        continueLoop();
        nested();
        onlyTheTriesInsideTheLoop();
        inCatch();
        inSwitch();
        doLoop();
    }
}
