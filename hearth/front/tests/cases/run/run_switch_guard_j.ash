package t;

sealed interface Shape permits Circle, Square { }
record Circle(int r) implements Shape { }
record Square(int side) implements Shape { }

public class Main {
    static int calls = 0;

    static bool big(int v) {
        calls++;
        return v > 100;
    }

    static String describe(Object o) {
        return switch (o) {
            case Integer i when i > 100 -> "big int " + i;
            case Integer i when i < 0 -> "negative " + i;
            case Integer i -> "int " + i;
            case String s when s.isEmpty() -> "empty string";
            case String s when s.length() > 3 && s.charAt(0) == 'x' -> "long x-string " + s;
            case String s -> "string " + s;
            default -> "other";
        };
    }

    static int area(Shape s) {
        return switch (s) {
            case Circle c when c.r() == 0 -> 0;
            case Circle c -> 3 * c.r() * c.r();
            case Square q when q.side() > 10 -> -1;
            case Square q -> q.side() * q.side();
        };
    }

    static String early(Object o) {
        switch (o) {
            case Integer i when big(i):
                return "guard called for " + i;
            case Integer i:
                return "plain " + i;
            default:
                return "not an int";
        }
    }

    static String sideEffect(Object o) {
        int n = 0;
        return switch (o) {
            case Integer i when (n = i * 2) > 10 -> "n=" + n;
            case Integer i when n++ == 0 -> "second, n=" + n + " i=" + i;
            default -> "none n=" + n;
        };
    }

    public static void main(String[] args) {
        Object[] xs = {5, 500, -3, "", "xyzzy", "abc", new Circle(1)};
        for (Object x : xs) System.out.println(describe(x));
        System.out.println(area(new Circle(0)) + " " + area(new Circle(2)) + " " + area(new Square(11)) + " " + area(new Square(3)));
        System.out.println(early(7));
        System.out.println(early(700));
        System.out.println(early("s"));
        System.out.println("guard calls: " + calls);     // the guard runs only for Integers
        System.out.println(sideEffect(9));
        System.out.println(sideEffect(1));
        System.out.println(sideEffect("q"));
    }
}
