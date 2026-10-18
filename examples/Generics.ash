package examples;

import ash.lang.function.Function;
import ash.util.ArrayList;
import ash.util.List;

class Pair<A, B> {
    private final A first;
    private final B second;

    Pair(A first, B second) {
        this.first = first;
        this.second = second;
    }

    A first() { return first; }
    B second() { return second; }

    <C> Pair<A, C> withSecond(C value) {
        return new Pair<>(first, value);
    }
}

class Stack<T> {
    private final List<T> items = new ArrayList<>();

    void push(T item) { items.add(item); }

    T pop() { return items.removeLast(); }

    bool isEmpty() { return items.isEmpty(); }
}

public class Generics {
    static <T extends Comparable<T>> T largest(List<T> xs) {
        T best = xs.get(0);
        for (T x : xs) {
            if (x.compareTo(best) > 0) {
                best = x;
            }
        }
        return best;
    }

    static <T, R> List<R> mapAll(List<T> xs, Function<T, R> f) {
        List<R> out = new ArrayList<>();
        for (T x : xs) {
            out.add(f.apply(x));
        }
        return out;
    }

    public static void main(String[] args) {
        Pair<String, Integer> p = new Pair<>("age", 42);
        Pair<String, Double> q = p.withSecond(1.5);
        System.out.println(p.first() + "=" + p.second() + " " + q.second());
        Stack<String> st = new Stack<>();
        st.push("a");
        st.push("b");
        System.out.println(st.pop() + st.pop() + st.isEmpty());
        List<Integer> nums = new ArrayList<>();
        nums.add(4);
        nums.add(11);
        nums.add(7);
        System.out.println(largest(nums));
        List<String> words = new ArrayList<>();
        words.add("pear");
        words.add("fig");
        System.out.println(largest(words) + " " + mapAll(words, w -> w.length()).get(0));
    }
}
