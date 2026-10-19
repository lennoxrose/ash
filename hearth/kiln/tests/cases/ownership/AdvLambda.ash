// Lambdas and captured variables: a variable a lambda assigns or captures has an unknown value for the range and
// initialisation checks, and a region loop must not let a lambda keep an iteration's data.
public class AdvLambda {
    interface IntFn { int apply(int x); }
    interface Sink { void put(String s); }

    static String last = "";
    static int calls = 0;

    static int run(IntFn f, int v) { calls++; return f.apply(v); }

    public static void main(String[] args) {
        int[] counter = new int[1];
        int n = 0;
        for (int i = 0; i < 3; i++) { n += run(x -> { counter[0] += x; return counter[0]; }, i + 1); }

        int[] a = new int[4];
        int idx = 0;
        idx = run(x -> x + 1, 2);                  // 3 at run time, unknown to the compiler
        a[idx] = 7;

        int k = 9;
        final int kk = k;
        int[] b = new int[3];
        for (int i = 0; i < 3; i++) b[i] = run(x -> x + kk, i) % 3;

        int late;
        Runnable r = () -> calls += 100;
        r.run();
        late = calls;                             // assigned before use
        a[late % 4] += 1;

        // region loops that call lambdas, keep strings through them, or only look like they keep nothing
        Sink sink = s -> last = s;
        long total = 0;
        for (int i = 0; i < 50000; i++) {
            String t = "row-" + i;
            total += t.length();
        }
        for (int i = 0; i < 50000; i++) {
            String t = "kept-" + i;
            sink.put(t);                          // keeps t through the lambda: not a region
            total += last.length();
        }
        String[] keep = new String[3];
        for (int i = 0; i < 3; i++) {
            String t = "slot-" + i + "-" + (i * 7919);
            keep[i] = t;                          // stored in an outer array
        }
        for (int i = 0; i < 20000; i++) { String junk = "junk" + i; total += junk.length(); }
        System.out.println(n + " " + counter[0] + " " + a[3] + " " + b[0] + b[1] + b[2] + " " + late + " " + total + " " + last);
        System.out.println(keep[0] + " " + keep[1] + " " + keep[2]);
    }
}
