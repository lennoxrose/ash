package t;

interface Calc {
    int apply(int x);

    static Calc doubler() {
        return x -> x * 2;
    }
}

class Outer {
    public const int BASE = 10;
    private static int made = 0;

    static class Inner {
        private final int v;

        Inner(int v) {
            this.v = v;
            made++;
        }

        int plusBase() { return v + BASE; }
    }

    static int count() { return made; }
}

class Shape {
    String show(int n) { return "int " + n; }
    String show(String s) { return "string " + s; }
    String show(int a, int b) { return "pair " + a + "," + b; }
    String show(double d) { return "double " + d; }
}

public class RunLang {
    static int sum(int... xs) {
        int t = 0;
        for (int x : xs) {
            t += x;
        }
        return t;
    }

    static int depth = 0;

    static int recurse(int n) {
        return n == 0 ? 0 : 1 + recurse(n - 1);
    }

    public static void main(String[] args) {
        Outer.Inner in = new Outer.Inner(5);
        System.out.println(in.plusBase() + " " + Outer.count() + " " + Outer.BASE);
        System.out.println(Calc.doubler().apply(21));
        Shape s = new Shape();
        System.out.println(s.show(1) + "|" + s.show("a") + "|" + s.show(2, 3) + "|" + s.show(1.5));
        System.out.println(sum() + " " + sum(4) + " " + sum(1, 2, 3));
        int x = 5;
        x <<= 2;
        x |= 1;
        System.out.println(x + " " + (x >> 1) + " " + (x & 6) + " " + (x ^ 3) + " " + (~x));
        long big = 1L << 40;
        System.out.println(big);
        String? t = null;
        String u = t == null ? "none" : t;
        System.out.println(u + " " + recurse(100));
        char c = 'x';
        System.out.println("x".equals(String.valueOf(c)) + " " + (c == 'x') + " " + (int) c);
    }
}
