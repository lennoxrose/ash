// Labeled break / continue across nested loops of every kind: for, while, do-while and for-each over arrays and lists,
// two and three levels deep, with the label on the inner or the outer loop. The expected output is what Java prints.
import ash.util.List;
import ash.util.ArrayList;

public class run_label_basic_j {
    static String log = "";
    static void note(String s) { log += s; }

    static int basic() {
        int n = 0;
        outer:
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                if (j == 3) continue outer;
                if (i == 3) break outer;
                n += i * 10 + j;
            }
        }
        return n;
    }

    static int whileDo() {
        int n = 0, i = 0;
        a:
        while (i < 6) {
            i++;
            int k = 0;
            b:
            do {
                k++;
                for (int j = 0; j < 4; j++) {
                    if (j == 2 && k == 1) continue b;
                    if (k == 3) continue a;
                    if (i == 5) break a;
                    n += j + k + i;
                }
            } while (k < 5);
            n += 1000;
        }
        return n;
    }

    static String fin() {
        String s = "";
        outer:
        for (int i = 0; i < 3; i++) {
            try {
                for (int j = 0; j < 3; j++) {
                    try {
                        if (j == 1 && i == 0) continue outer;
                        if (j == 2 && i == 1) break outer;
                        s += "b" + i + j + ";";
                    } finally {
                        s += "f" + i + j + ";";
                    }
                }
            } finally {
                s += "F" + i + ";";
            }
        }
        return s;
    }

    static int sw() {
        int n = 0;
        outer:
        for (int i = 0; i < 4; i++) {
            switch (i) {
                case 0: n += 1; break;
                case 1:
                    for (int j = 0; j < 3; j++) {
                        if (j == 1) continue outer;
                        n += 10;
                    }
                    break;
                case 2:
                    n += 100;
                    if (n > 0) break outer;
                    n += 5;
                    break;
                default:
                    n += 1000;
            }
            n += 7;
        }
        return n;
    }

    static int sw2() {
        int n = 0;
        outer:
        for (int i = 0; i < 4; i++) {
            switch (i) {
                case 1 -> { if (n > 0) continue outer; n += 3; }
                case 3 -> { break outer; }
                default -> n += 10;
            }
            n += 1;
        }
        return n;
    }

    static int foreach() {
        int[] a = {1, 2, 3, 4};
        int[] b = {10, 20, 30};
        int n = 0;
        outer:
        for (int x : a) {
            for (int y : b) {
                if (y == 20 && x == 2) continue outer;
                if (x == 4) break outer;
                n += x * y;
            }
        }
        return n;
    }

    static int three() {
        int n = 0;
        x:
        for (int i = 0; i < 3; i++) {
            y:
            for (int j = 0; j < 3; j++) {
                for (int k = 0; k < 3; k++) {
                    if (k == 1) continue y;
                    if (j == 2) continue x;
                    if (i == 2) break x;
                    n += 100 * i + 10 * j + k;
                }
                n += 1000000;
            }
            n += 5000;
        }
        return n;
    }

    static int infinite() {
        int n = 0;
        outer:
        while (true) {
            for (;;) {
                n++;
                if (n > 5) break outer;
                if (n % 2 == 0) continue outer;
            }
        }
        return n;
    }

    static int listForeach() {
        List<Integer> xs = new ArrayList<>();
        List<String> ys = new ArrayList<>();
        for (int i = 0; i < 5; i++) { xs.add(i); ys.add("y" + i); }
        int n = 0;
        outer:
        for (int x : xs) {
            for (String y : ys) {
                if (y.length() == 2 && x == 1) continue outer;
                if (x == 3) break outer;
                n += x * 10 + y.length();
            }
            n += 1000;
        }
        return n;
    }

    public static void main(String[] args) {
        System.out.println(listForeach());
        System.out.println(basic());
        System.out.println(whileDo());
        System.out.println(fin());
        System.out.println(sw());
        System.out.println(sw2());
        System.out.println(foreach());
        System.out.println(three());
        System.out.println(infinite());
    }
}
