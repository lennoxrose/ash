// try / catch / finally inside loops whose bodies otherwise qualify (jown/region.c): the exception raised in the
// body is a runtime error or a library exception, made and caught inside the iteration. Every loop must be a region.
import ash.util.ArrayList;
import ash.util.List;

public class RegionTry {
    static int[] data = {5, 4, 3, 2, 1, 0};

    static int risky(int i) {
        String s = "r" + i;
        try {
            int[] a = new int[3];
            a[i % 5] = 7;
            return a[0] + s.length();
        } catch (IndexOutOfBoundsException e) {
            return -s.length();
        }
    }

    static long divisions(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            String label = "d" + i;
            try {
                total += 100 / data[i % data.length];
                total += label.length();
            } catch (ArithmeticException e) {
                String m = "<" + e.getMessage() + ">";
                total += m.length() + label.length();
                if (i % 7 == 0) System.out.println("caught " + m + " at " + label);
            }
        }
        return total;
    }

    static long indexes(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            int[] small = new int[4];
            try {
                small[i % 6] = i;
                total += small[i % 4];
            } catch (RuntimeException e) {
                String why = "bad-" + i + ":" + e.getMessage();
                total += why.length();
                System.out.println(why);
            } finally {
                String done = "f" + i;
                total += done.length();
            }
        }
        return total;
    }

    static long nested(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            try {
                String a = "a" + i;
                try {
                    total += 10 / (i % 3);
                    total += a.length();
                } catch (ArithmeticException inner) {
                    total += 1000;
                    total += data[(i + 4) % 7];       // may raise again, caught by the outer catch
                }
            } catch (IndexOutOfBoundsException outer) {
                String o = "outer" + i + outer.getMessage();
                total += o.length();
            }
        }
        return total;
    }

    static long viaCallee(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) total += risky(i);
        return total;
    }

    static long withContinue(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            String s = "c" + i;
            try {
                if (i % 3 == 0) continue;
                total += 50 / (i % 4);
                if (i > n - 3) break;
            } catch (ArithmeticException e) {
                total += s.length();
            }
            total += s.length();
        }
        return total;
    }

    static long forEachTry() {
        long total = 0;
        for (int d : data) {
            String s = "e" + d;
            try {
                total += 60 / d + s.length();
            } catch (ArithmeticException e) {
                total += ("<" + e.getMessage() + ">").length();
            }
        }
        return total;
    }

    static int returnsInTry(int n) {
        for (int i = 0; i < n; i++) {
            String s = "x" + i;
            try {
                if (i == n - 1) return s.length() + 100 / (n - 1 - i + 1);
            } finally {
                String t = s + "!";
                if (t.length() > 100) System.out.println(t);
            }
        }
        return -1;
    }

    static String uncaught(int n) {
        // the exception is not caught in the iteration: it leaves the loop and is caught here, after the loop
        long total = 0;
        try {
            for (int i = 0; i < n; i++) {
                String s = "u" + i;
                try {
                    total += data[i] + s.length();
                } catch (ArithmeticException e) {
                    total = -1;
                }
            }
        } catch (IndexOutOfBoundsException e) {
            return "left the loop with " + total + ": " + e.getMessage();
        }
        return "no error " + total;
    }

    static long afterwards(int n) {
        long total = divisions(n) + indexes(n);
        String keep = "keep-" + total;
        long more = 0;
        for (int i = 0; i < n; i++) {
            String w = "w" + i + keep;
            try {
                more += w.length() / (i % 2);
            } catch (ArithmeticException e) {
                more += 1;
            }
        }
        return keep.length() + more;
    }

    public static void main(String[] args) {
        System.out.println(divisions(20));
        System.out.println(indexes(14));
        System.out.println(nested(12));
        System.out.println(viaCallee(11));
        System.out.println(withContinue(12));
        System.out.println(forEachTry());
        System.out.println(returnsInTry(5));
        System.out.println(uncaught(9));
        System.out.println(afterwards(15));
    }
}
