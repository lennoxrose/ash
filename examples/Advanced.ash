package examples;

import ash.lang.Iterable;
import ash.util.Iterator;
import ash.util.Optional;
import ash.util.Result;

interface Greeter {
    String greet(String who);

    static Greeter polite() {
        return who -> "Good day, " + who;
    }
}

class Countdown implements Iterable<Integer> {
    private final int from;

    Countdown(int from) { this.from = from; }

    public Iterator<Integer> iterator() {
        return new Cursor(from);
    }

    static class Cursor implements Iterator<Integer> {
        private int next;

        Cursor(int start) { this.next = start; }

        public bool hasNext() { return next > 0; }

        public Integer next() {
            int v = next;
            next--;
            return v;
        }
    }
}

class Printer {
    String show(int n) { return "int:" + n; }
    String show(double d) { return "double:" + d; }
    String show(String s) { return "text:" + s; }
    String show(int a, int b) { return "pair:" + a + "-" + b; }
}

public class Advanced {
    static Result<Integer, String> safeDivide(int a, int b) {
        if (b == 0) {
            return Result.err("divide by zero");
        }
        return Result.ok(a / b);
    }

    static Optional<String> firstWord(String text) {
        if (text.isEmpty()) {
            return Optional.empty();
        }
        return Optional.of(text.split(" ")[0]);
    }

    static void fail() {
        try {
            throw new IllegalStateException("inner");
        } catch (IllegalStateException e) {
            throw new RuntimeException("outer", e);
        }
    }

    public static void main(String[] args) {
        System.out.println(Greeter.polite().greet("Ash"));
        int total = 0;
        for (int v : new Countdown(4)) {
            total = total * 10 + v;
        }
        System.out.println(total);
        Printer p = new Printer();
        System.out.println(p.show(1) + " " + p.show(2.5) + " " + p.show("x") + " " + p.show(3, 4));
        System.out.println(safeDivide(9, 3).unwrap() + " " + safeDivide(1, 0).getError());
        System.out.println(firstWord("hello world").get() + " " + firstWord("").isPresent());
        try {
            fail();
        } catch (RuntimeException e) {
            Throwable? cause = e.getCause();
            System.out.println(e.getMessage() + " <- " + (cause == null ? "none" : cause.getMessage()));
        }
    }
}
