package t;

sealed interface Shape permits Circle, Square { }
record Circle(int r) implements Shape { }
record Square(int side) implements Shape { }

public class Main {
    static String f(Object o) {
        return switch (o) {
            case Integer i when i + 1 -> "not a bool";
            case String s when s.isEmpty() -> "empty";
            case Integer j when q > 2 -> "unknown name";
            case null, Integer k when true -> "shared";
        };
    }

    static String g(Object o) {
        return switch (o) {
            case Integer i when i > 0 -> "positive";
            case String s -> "string";
        };
    }

    static int h(Shape s) {
        return switch (s) {
            case Circle c when c.r() > 0 -> 1;      // a guarded case covers nothing: Circle is missing
            case Square q -> 2;
        };
    }

    public static void main(String[] args) {
        System.out.println(f(1) + g(2));
    }
}
