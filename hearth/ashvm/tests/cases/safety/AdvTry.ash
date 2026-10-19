// try / catch / finally with ranges, initialisation and region loops (exceptions that leave an iteration, finally
// blocks that run after a return value was computed, values that outlive the loop through a catch).
public class AdvTry {
    static String saved = "";
    static int boom(int i) { if (i % 3 == 0) throw new IllegalStateException("boom-" + i); return i; }
    static int div(int a, int b) { return a / b; }

    static int viaFinally(int n) {
        int result;
        try {
            result = n * 2;
            if (n > 2) return result + 1000;
        } finally {
            saved = "fin-" + n;
        }
        return result;
    }

    static String catchKeeps(int rounds) {
        String out = "";
        for (int i = 0; i < rounds; i++) {
            try {
                boom(i);
            } catch (IllegalStateException e) {
                String? m = e.getMessage();
                if (m != null) out = m;             // stored in an outer variable: not a region
            }
        }
        return out;
    }

    static long churn(int rounds) {
        long total = 0;
        for (int i = 0; i < rounds; i++) {
            String s = "t" + i;
            int[] tmp = new int[4];
            try {
                tmp[i % 4] = s.length();
                total += div(100, i % 5) + tmp[i % 4];
            } catch (ArithmeticException e) {
                String? m = e.getMessage();
                total += 1 + (m == null ? 0 : m.length());
            } finally {
                total += s.length();
            }
        }
        return total;
    }

    public static void main(String[] args) {
        int[] a = new int[3];
        int j;
        try {
            j = 2;
            a[j] = 1;
        } catch (RuntimeException e) {
            j = 0;
        }
        a[j] += 1;                                   // j is 2 or 0 on every path
        int z = 1;
        try {
            z = 7;
            a[z] = 1;                                // always out of bounds, inside a try with a catch: quiet
        } catch (IndexOutOfBoundsException e) {
            z = 2;
        } finally {
            a[0] += 1;
        }
        a[z] += 5;                                   // z is 7 (never) or 2
        System.out.println(viaFinally(1) + " " + viaFinally(3) + " " + saved);
        System.out.println(catchKeeps(10));
        System.out.println(churn(30000));
        System.out.println(a[0] + " " + a[1] + " " + a[2]);
    }
}
