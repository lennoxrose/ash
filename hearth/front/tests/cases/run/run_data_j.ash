package t;

import ash.util.List;
import ash.util.ArrayList;
import ash.util.Iterator;
import ash.util.Map;
import ash.util.HashMap;
import ash.lang.function.Function;
import ash.lang.function.Predicate;

enum Level {
    LOW(1), MID(5), HIGH(10);

    private final int weight;

    Level(int weight) {
        this.weight = weight;
    }

    int getWeight() { return weight; }
}

record Point(int x, int y) {
    int dist() { return Math.abs(x) + Math.abs(y); }
}

class Box<T> {
    private T value;

    Box(T value) { this.value = value; }
    T get() { return value; }
    void set(T v) { this.value = v; }
    <R> Box<R> map(Function<T, R> f) { return new Box<>(f.apply(value)); }
}

class Range implements Iterable<Integer> {
    private final int lo;
    private final int hi;

    Range(int lo, int hi) {
        this.lo = lo;
        this.hi = hi;
    }

    public Iterator<Integer> iterator() {
        return new RangeIter(lo, hi);
    }
}

class RangeIter implements Iterator<Integer> {
    private int cur;
    private final int end;

    RangeIter(int cur, int end) {
        this.cur = cur;
        this.end = end;
    }

    public bool hasNext() { return cur < end; }

    public Integer next() {
        int v = cur;
        cur++;
        return v;
    }
}

public class RunData {
    static int twice(int x) { return x * 2; }

    public static void main(String[] args) {
        List<Integer> xs = new ArrayList<>();
        for (int i = 1; i <= 6; i++) {
            xs.add(i * i);
        }
        System.out.println(xs.size() + " " + xs.get(2) + " " + xs.contains(16));
        int sum = 0;
        for (int v : xs) {
            sum += v;
        }
        System.out.println(sum);
        List<Integer> big = xs.stream().filter(v -> v > 10).toList();
        System.out.println(big.size() + " " + big.get(0));
        List<String> labels = xs.stream().map(v -> "n" + v).toList();
        System.out.println(labels.get(5));
        int total = xs.stream().reduce(0, (a, b) -> a + b);
        System.out.println(total);

        Map<String, Integer> counts = new HashMap<>();
        String[] words = "a b a c b a".split(" ");
        for (String w : words) {
            Integer? old = counts.get(w);
            counts.put(w, old == null ? 1 : old + 1);
        }
        System.out.println(counts.get("a") + " " + counts.get("b") + " " + counts.get("c") + " " + counts.size());
        for (String k : counts.keys()) {
            System.out.println(k + "=" + counts.get(k));
        }

        for (Level l : Level.values()) {
            System.out.println(l + " " + l.ordinal() + " " + l.getWeight());
        }
        Level m = Level.valueOf("MID");
        String d = switch (m) {
            case LOW -> "l";
            case MID -> "m";
            case HIGH -> "h";
        };
        System.out.println(d);

        Point p = new Point(3, -4);
        Point q = new Point(3, -4);
        System.out.println(p + " " + p.dist() + " " + p.equals(q) + " " + (p == q) + " " + p.x());

        Box<Integer> bi = new Box<>(20);
        Box<String> bs = bi.map(v -> "v" + v);
        System.out.println(bs.get() + " " + bi.get());

        int acc = 0;
        for (int v : new Range(0, 5)) {
            acc += v;
        }
        System.out.println(acc);

        Function<Integer, Integer> f = RunData::twice;
        Predicate<String> empty = String::isEmpty;
        System.out.println(f.apply(21) + " " + empty.test("") + " " + empty.test("x"));

        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 3; i++) {
            sb.append("i").append(i).append(',');
        }
        System.out.println(sb.toString());
        String s = "Hello, World";
        System.out.println(s.toUpperCase() + " " + s.length() + " " + s.substring(7) + " " + s.indexOf("World") + " " + s.charAt(1));
        char c = 'a';
        c++;
        System.out.println(c);
        double avg = 7 / 2;
        double real = 7 / 2.0;
        System.out.println(avg + " " + real + " " + (long) 3.99 + " " + (int) -3.99);
        int[][] grid = new int[2][3];
        grid[1][2] = 9;
        System.out.println(grid[1][2] + grid[0][0] + grid.length + grid[0].length);
        int n = 0;
        do {
            n += 3;
        } while (n < 10);
        System.out.println(n);
    }
}
