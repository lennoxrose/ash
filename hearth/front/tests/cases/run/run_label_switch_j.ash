// Labeled jumps out of switches: if-chain switches, switches that need the one-shot loop (fall through, early break),
// arrow form, strings and enums, nested in each other and in try / finally.
public class run_label_switch_j {
    enum Color { RED, GREEN, BLUE }

    static String log = "";

    static int fallThrough() {
        int n = 0;
        outer:
        for (int i = 0; i < 6; i++) {
            switch (i) {
                case 0:
                    n += 1;
                case 1:
                    n += 10;
                    if (i == 1) continue outer;
                    break;
                case 2:
                    n += 100;
                    if (n > 50) break outer;
                    n += 1000000;
                default:
                    n += 5000;
            }
            n += 7;
        }
        return n;
    }

    static int earlyBreak() {
        int n = 0;
        outer:
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 3; j++) {
                switch (j) {
                    case 0:
                        if (i == 2) break;                 // leaves the switch only
                        n += 1;
                        break;
                    case 1:
                        if (i == 3) continue outer;
                        n += 20;
                        break;
                    default:
                        if (i == 4) break outer;
                        n += 300;
                }
                n += 4000;
            }
        }
        return n;
    }

    static int nestedSwitches() {
        int n = 0;
        outer:
        for (int i = 0; i < 4; i++) {
            inner:
            for (int j = 0; j < 4; j++) {
                switch (i) {
                    case 0:
                        switch (j) {
                            case 1: continue inner;
                            case 2: continue outer;
                            default: n += 1;
                        }
                        break;
                    case 1:
                        switch (j) {
                            case 0: n += 20;
                            case 1: n += 30; break;
                            default: break inner;
                        }
                        break;
                    case 2:
                        n += 400;
                        if (j == 3) break outer;
                        break;
                    default:
                        n += 5000;
                }
                n += 60000;
            }
        }
        return n;
    }

    static String strings() {
        String[] names = {"a", "bb", "ccc", "dd", "e"};
        String s = "";
        outer:
        for (String nm : names) {
            for (int k = 0; k < 3; k++) {
                switch (nm) {
                    case "a": s += "A"; break;
                    case "bb": if (k == 1) continue outer; s += "B"; break;
                    case "ccc": s += "C"; if (k == 2) break outer; break;
                    default: s += "?";
                }
            }
            s += "|";
        }
        return s;
    }

    static int enums() {
        Color[] cs = {Color.RED, Color.GREEN, Color.BLUE, Color.RED};
        int n = 0;
        outer:
        for (int r = 0; r < 3; r++) {
            for (Color c : cs) {
                switch (c) {
                    case RED -> n += 1;
                    case GREEN -> { if (r == 1) continue outer; n += 10; }
                    case BLUE -> { if (r == 2) break outer; n += 100; }
                }
            }
        }
        return n;
    }

    static int arrowExpr() {
        int n = 0;
        outer:
        for (int i = 0; i < 6; i++) {
            int v = switch (i % 3) {
                case 0 -> 1;
                case 1 -> 20;
                default -> 300;
            };
            switch (v) {
                case 1 -> n += 1;
                case 20 -> { if (i > 3) continue outer; n += 20; }
                default -> { if (i == 5) break outer; n += 300; }
            }
            n += 5000;
        }
        return n;
    }

    static String inTry() {
        outer:
        for (int i = 0; i < 4; i++) {
            try {
                switch (i) {
                    case 0:
                        log += "z";
                    case 1:
                        log += "o";
                        if (i == 1) continue outer;
                        break;
                    case 2:
                        for (int j = 0; j < 2; j++) {
                            try {
                                if (j == 1) break outer;
                            } finally {
                                log += "j" + j;
                            }
                        }
                        break;
                    default:
                        log += "d";
                }
                log += "e";
            } finally {
                log += "F" + i + ";";
            }
        }
        return log;
    }

    public static void main(String[] args) {
        System.out.println(fallThrough());
        System.out.println(earlyBreak());
        System.out.println(nestedSwitches());
        System.out.println(strings());
        System.out.println(enums());
        System.out.println(arrowExpr());
        System.out.println(inTry());
    }
}
