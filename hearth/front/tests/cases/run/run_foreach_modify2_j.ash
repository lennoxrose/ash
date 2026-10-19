package t;

import ash.util.ArrayList;
import ash.util.ConcurrentModificationException;
import ash.util.List;

// The ConcurrentModificationException check is left out of a for-each whose body provably cannot resize a list
// (only reads, `set`, String / Math calls, printing). Any other body keeps the check, however indirectly it
// changes the list: through a project method, a static field, a lambda, a field or a constructor.
public class RunForeachModify2 {
    static List<Integer> shared = new ArrayList<>();

    static List<Integer> make(int n) {
        List<Integer> xs = new ArrayList<>();
        for (int i = 1; i <= n; i++) xs.add(i);
        return xs;
    }

    static void push(List<Integer> l, int v) { l.add(v); }
    static void pushShared(int v) { shared.add(v); }

    static void quiet() {
        List<Integer> xs = make(4);
        int sum = 0;
        for (int x : xs) {                      // reads, Math and a nested for-each: no check needed
            sum += Math.max(x, 2);
            for (int y : xs) sum += y;
        }
        for (int x : xs) xs.set(0, x + 100);    // set keeps the size
        System.out.println("quiet: " + sum + " " + xs.get(0) + " " + xs.size());
    }

    static void viaMethod() {
        List<Integer> xs = make(3);
        int seen = 0;
        try {
            for (int x : xs) { seen++; push(xs, x); }
            System.out.println("viaMethod: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("viaMethod: CME after " + seen + ": " + e.getMessage());
        }
    }

    static void viaField() {
        shared = make(3);
        int seen = 0;
        try {
            for (int x : shared) { seen++; pushShared(x); }
            System.out.println("viaField: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("viaField: CME after " + seen);
        }
    }

    static void viaLambda() {
        List<Integer> xs = make(3);
        Runnable r = () -> xs.add(9);
        int seen = 0;
        try {
            for (int x : xs) { seen++; r.run(); }
            System.out.println("viaLambda: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("viaLambda: CME after " + seen);
        }
    }

    static void viaNested() {
        List<Integer> xs = make(3);
        int seen = 0;
        try {
            for (int x : xs) {
                seen++;
                for (int y : make(1)) { xs.remove(0); }   // the inner loop is quiet about its own list only
            }
            System.out.println("viaNested: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("viaNested: CME after " + seen);
        }
    }

    public static void main(String[] args) {
        quiet();
        viaMethod();
        viaField();
        viaLambda();
        viaNested();
    }
}
