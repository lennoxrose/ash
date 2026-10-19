package t;

import ash.util.ArrayList;
import ash.util.ConcurrentModificationException;
import ash.util.List;

// A for-each over an ArrayList follows Java's iterator: the loop ends when the position reaches the size, and a
// size that changed since the loop began is a ConcurrentModificationException when the next element is asked for.
// `set` does not change the size and is fine. Both backends must agree (they share this lowering).
public class RunForeachModify {
    static List<Integer> make(int n) {
        List<Integer> xs = new ArrayList<>();
        for (int i = 1; i <= n; i++) xs.add(i);
        return xs;
    }

    static void grow() {
        List<Integer> xs = make(3);
        int seen = 0;
        try {
            for (int x : xs) {
                seen++;
                xs.add(x * 10);
            }
            System.out.println("grow: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("grow: CME after " + seen + ", size " + xs.size());
        }
    }

    static void removeFirst() {
        List<Integer> xs = make(5);
        StringBuilder sb = new StringBuilder();
        try {
            for (int x : xs) {
                sb.append(x).append(' ');
                if (x == 2) xs.remove(0);
            }
            System.out.println("removeFirst: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("removeFirst: CME after " + sb.toString().trim() + ", size " + xs.size());
        }
    }

    static void removeSecondToLast() {
        List<Integer> xs = make(4);
        StringBuilder sb = new StringBuilder();
        for (int x : xs) {
            sb.append(x).append(' ');
            if (x == 3) xs.remove(0);          // the position then equals the new size: the loop just ends
        }
        System.out.println("removeSecondToLast: " + sb.toString().trim() + ", size " + xs.size());
    }

    static void removeLast() {
        List<Integer> xs = make(3);
        int seen = 0;
        try {
            for (int x : xs) {
                seen++;
                if (x == 3) xs.remove(2);
            }
            System.out.println("removeLast: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("removeLast: CME after " + seen);
        }
    }

    static void setIsFine() {
        List<Integer> xs = make(4);
        int total = 0;
        for (int x : xs) {
            if (x == 2) xs.set(3, 40);
            total += x;
        }
        System.out.println("setIsFine: " + total + " " + xs.get(3));
    }

    static void leaveAfterChange() {
        List<Integer> xs = make(4);
        for (int x : xs) {
            if (x == 2) {
                xs.add(99);
                break;                          // leaving right after the change is fine
            }
        }
        int rest = 0;
        for (int x : xs) {
            if (x == 1) continue;
            rest += x;
        }
        System.out.println("leaveAfterChange: " + xs.size() + " " + rest);
    }

    static void nestedReads() {
        List<Integer> xs = make(3);
        int total = 0;
        for (int a : xs) for (int b : xs) total += a * b;
        List<Integer> none = new ArrayList<>();
        for (int x : none) total += 1000;
        System.out.println("nestedReads: " + total);
    }

    static void growFromInner() {
        List<Integer> xs = make(2);
        List<Integer> ys = new ArrayList<>();
        try {
            for (int a : xs) {
                for (int b : xs) ys.add(a + b);
                xs.add(a);
            }
        } catch (ConcurrentModificationException e) {
            System.out.println("growFromInner: CME, ys " + ys.size() + ", xs " + xs.size());
        }
    }

    public static void main(String[] args) {
        grow();
        removeFirst();
        removeSecondToLast();
        removeLast();
        setIsFine();
        leaveAfterChange();
        nestedReads();
        growFromInner();
        List<Integer> xs = make(2);
        try {
            for (int x : xs) xs.add(x);
            System.out.println("not reached");
        } catch (RuntimeException e) {
            System.out.println("main: " + (e instanceof ConcurrentModificationException));
        }
    }
}
