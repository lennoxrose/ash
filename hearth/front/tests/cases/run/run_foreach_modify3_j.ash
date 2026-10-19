package t;

import ash.util.ArrayList;
import ash.util.ConcurrentModificationException;
import ash.util.List;

// The size check of a for-each over a list is left out when the body provably cannot resize the list: a call to
// project code whose body (transitively) cannot, and add / remove on a list nobody else can reference. Every way
// of reaching the walked list from the body is still caught: an alias, a list handed to a method, a field, a
// lambda, the same local, a call chain or a recursion that ends in an `add`.
public class RunForeachModify3 {
    static List<Integer> shared = new ArrayList<>();
    static List<Integer> kept;
    static int calls = 0;

    static void keep(List<Integer> l) { kept = l; }
    static void grow(List<Integer> l) { l.add(1); }
    static int twice(int v) { calls++; return v * 2; }
    static int even(int n) { return n == 0 ? 1 : odd(n - 1); }
    static int odd(int n) { return n == 0 ? 0 : even(n - 1); }
    static void chainA(int v) { chainB(v); }
    static void chainB(int v) { chainC(v); }
    static void chainC(int v) { shared.add(v); }
    static void ping(int n) { if (n > 0) pong(n - 1); else shared.add(0); }
    static void pong(int n) { ping(n); }
    static List<Integer> build(int n) {
        List<Integer> out = new ArrayList<>();
        for (int i = 0; i < n; i++) out.add(i);
        return out;
    }
    static List<Integer> fill(List<Integer> l, int n) { for (int i = 0; i < n; i++) l.add(i); return l; }

    static void fast() {
        List<Integer> src = build(5);
        List<Integer> copy = new ArrayList<>();
        List<Integer> evens = new ArrayList<>();
        int total = 0;
        for (int x : src) {                       // adds to other fresh lists, calls harmless project code
            copy.add(twice(x));
            if (even(x) == 1) evens.add(x);
            total += odd(x);
        }
        List<Integer> mine = new ArrayList<>();
        for (int i = 0; i < 4; i++) mine.add(i);
        for (int x : mine) {                       // nobody else can reach `mine`: any call is fine
            System.out.println("mine " + x + " " + twice(x));
            for (int y : mine) total += y;
        }
        for (int x : mine) mine.set(0, x + 10);
        int csum = 0;
        for (int x : copy) csum += x;
        System.out.println("fast: " + copy.size() + " " + csum + " " + evens.size() + " " + total + " " + mine.get(0) + " calls=" + calls);
    }



    static void sameLocal() {
        List<Integer> a = new ArrayList<>();
        a.add(1); a.add(2);
        int seen = 0;
        try {
            for (int x : a) { seen++; a.add(x); }
            System.out.println("sameLocal: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("sameLocal: CME after " + seen);
        }
    }

    static void passedToCallee() {
        List<Integer> a = new ArrayList<>();
        a.add(1); a.add(2);
        int seen = 0;
        try {
            for (int x : a) { seen++; grow(&a); }
            System.out.println("passedToCallee: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("passedToCallee: CME after " + seen);
        }
    }

    static void viaChain() {
        shared = fill(new ArrayList<>(), 3);
        int seen = 0;
        try {
            for (int x : shared) { seen++; chainA(x); }
            System.out.println("viaChain: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("viaChain: CME after " + seen);
        }
    }

    static void viaRecursion() {
        shared = fill(new ArrayList<>(), 3);
        int seen = 0;
        try {
            for (int x : shared) { seen++; ping(3); }
            System.out.println("viaRecursion: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("viaRecursion: CME after " + seen);
        }
    }

    static void viaLambda() {
        List<Integer> a = new ArrayList<>();
        a.add(1); a.add(2);
        Runnable r = () -> a.add(9);
        int seen = 0;
        try {
            for (int x : a) { seen++; r.run(); }
            System.out.println("viaLambda: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("viaLambda: CME after " + seen);
        }
    }


    static void nestedOwn() {
        List<Integer> outer = new ArrayList<>();
        outer.add(1); outer.add(2);
        int seen = 0;
        try {
            for (int x : outer) {
                seen++;
                List<Integer> inner = new ArrayList<>();
                inner.add(x);
                for (int y : inner) { inner.add(y); }      // its own list, grown while it is walked
            }
            System.out.println("nestedOwn: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("nestedOwn: CME after " + seen);
        }
    }

    public static void main(String[] args) {
        fast();
        sameLocal();
        passedToCallee();
        viaChain();
        viaRecursion();
        viaLambda();
        nestedOwn();
    }
}
