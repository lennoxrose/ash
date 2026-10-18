package demo;

import ash.util.List;
import ash.util.ArrayList;
import ash.util.Map;
import ash.util.HashMap;

// ---------------------------------------------------------------------------
// main.ash - living showcase of the new Java-like Ash syntax.
// Updated after each larger migration milestone (see MIGRATION.md).
//
// Milestone status
//   [x] Phase 0  specification locked (MIGRATION.md decisions D1-D15)
//   [x] Phase 1  lexer: new keywords, // /* */ /// comments
//   [x] Phase 2  parser + AST: everything in this file parses
//   [x] Phase 3  type checker: `ashvm check main.ash` passes
//                (types, nullability, generics, overloads, exceptions, switch)
//   [x] Phase 4  lowering: `ashvm run main.ash`, `kiln main.ash -o main` (native)
//   [x] Phase 5  stdlib: lang util io json math cli test time concurrent net web game desktop
//                (the last five are pure-Ash models; real OS access needs new VM builtins)
//   [x] Phase 6  examples/ (18 programs), README, API docs on every stdlib declaration
// ---------------------------------------------------------------------------

/// A sealed interface with a default method.
public sealed interface Shape permits Circle, Square {
    double area();

    default String describe() {
        return "shape with area " + area();
    }
}

/// Records are immutable value types.
record Circle(double radius) implements Shape {
    public double area() {
        return 3.14159 * radius * radius;
    }
}

record Square(double side) implements Shape {
    Square {
        if (side < 0) {
            throw new IllegalArgumentException("negative side");
        }
    }

    public double area() {
        return side * side;
    }
}

enum Level {
    LOW(1), HIGH(3);

    private final int weight;

    Level(int weight) {
        this.weight = weight;
    }

    public int getWeight() {
        return weight;
    }
}

public class Main {
    private static final int LIMIT = 5;

    public static int fib(int n) {
        return n < 2 ? n : fib(n - 1) + fib(n - 2);
    }

    static String name(Shape s) {
        return switch (s) {
            case Circle c -> "circle " + c.radius();
            case Square q -> "square " + q.side();
        };
    }

    static <T> T pick(T a, T b, bool first) {
        return first ? a : b;
    }

    public static void main(String[] args) {
        Function<Integer, Integer> twice = v -> v * 2;
        System.out.println(name(new Circle(1.0)) + " " + twice.apply(pick(2, 3, true)));
        List<Shape> shapes = new ArrayList<>();
        shapes.add(new Circle(1.5));
        shapes.add(new Square(2.0));

        for (Shape s : shapes) {
            System.out.println(s.describe());
        }

        Map<String, Integer> counts = new HashMap<>();
        counts.put("fib", fib(10));

        String? maybe = counts.containsKey("x") ? "present" : null;
        if (maybe != null) {
            System.out.println(maybe.length());
        }

        try {
            Shape bad = new Square(-1.0);
        } catch (IllegalArgumentException e) {
            System.out.println("caught: " + e.getMessage());
        }

        int total = 0;
        for (int i = 0; i < 10; i++) {
            if (i == 3) { continue; }
            if (i == LIMIT + 2) { break; }
            total += i;
        }

        String label = switch (Level.HIGH) {
            case LOW -> "low";
            case HIGH -> "high";
        };
        System.out.println(label + " " + total);
    }
}
