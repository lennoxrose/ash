// Variables declared inside a loop body are fresh on every iteration: a catch variable, an `instanceof`
// binding and a switch type-pattern binding may be handed to a project method that keeps them, in every
// iteration, without the back edge being read as a second use of a moved value.
public class OwnLoopFresh {
    static Object kept;
    static ArithmeticException keptEx;
    static int keepEx(ArithmeticException e) { keptEx = e; return 1; }
    static int keep(Object o) { kept = o; return 1; }
    static int boom(int i) { return 10 / i; }

    public static void main(String[] args) {
        int total = 0;
        for (int i = 0; i < 4; i++) {
            try {
                total += boom(i % 2);
            } catch (ArithmeticException e) {
                total += keepEx(e);
            }
        }
        Object[] xs = { "a", new int[1], "bb", "c" };
        for (int i = 0; i < xs.length; i++) {
            Object o = xs[i];
            if (o instanceof String s) total += keep(s);
            switch (o) {
                case String t -> total += keep(t) * 10;
                default -> total += 100;
            }
        }
        int n = 0;
        while (n < 3) {
            n++;
            try {
                total += boom(0);
            } catch (ArithmeticException e) {
                total += keepEx(e) * 1000;
            } finally {
                int[] f = new int[2];
                f[0] = n;
                total += f[0];
            }
        }
        System.out.println(total);
    }
}
