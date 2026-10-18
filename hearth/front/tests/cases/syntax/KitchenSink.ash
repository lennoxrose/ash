package demo.app;

import ash.util.List;
import ash.util.*;

/// A documentation comment.
public sealed interface Shape permits Circle, Square {
    double area();
    default String describe() {
        return "shape with area " + area();
    }
    static Shape unit() {
        return new Square(1.0);
    }
}

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

enum Color {
    RED(1), GREEN(2), BLUE(3);

    private final int code;

    Color(int code) {
        this.code = code;
    }

    public int getCode() {
        return code;
    }
}

abstract class Animal<T extends Comparable<T>> {
    protected final String name;
    private int legs = 4, tails = 1;
    static final int MAX = 10;
    const int LIMIT = 5;

    protected Animal(String name) {
        this.name = name;
    }

    public abstract String speak();

    @Override
    public String toString() {
        return name + ":" + speak();
    }
}

public class KitchenSink extends Animal<String> implements Runnable {
    private String? nickname = null;
    private int[] data = new int[10];
    private int[][] grid = {{1, 2}, {3, 4}};

    static {
        System.out.println("static init");
    }

    public KitchenSink() {
        super("sink");
    }

    public String speak() {
        return "glug";
    }

    public void run() {
        int x = 5;
        long y = 10L;
        var z = x + y * 2;
        final double d = 1.5e3;
        char c = 'a';
        bool flag = x > 3 && !(y < 2) || false;
        x += 3;
        x >>= 1;
        x = x >>> 2 << 1;
        List<Map<String, Integer>> nested = new ArrayList<>();
        int r = flag ? 1 : 2;
        String? s = (String?) null;
        int cast = (int) d;
        if (x > 3) {
            x++;
        } else if (x < 0) {
            x--;
        } else {
            x = 0;
        }
        for (int i = 0; i < 10; i++) {
            if (i == 3) { continue; }
            if (i == 7) { break; }
        }
        for (String item : nested.stream().map(m -> m.toString()).toList()) {
            System.out.println(item);
        }
        while (x < 100) { x *= 2; }
        do { x--; } while (x > 50);
        switch (x) {
            case 1:
            case 2:
                x = 3;
                break;
            default:
                x = 4;
        }
        String label = switch (Color.RED) {
            case RED -> "r";
            case GREEN, BLUE -> "gb";
        };
        Object o = label;
        if (o instanceof String str && str.length() > 0) {
            System.out.println(str);
        }
        try {
            throw new RuntimeException("x");
        } catch (IllegalArgumentException | IllegalStateException e) {
            System.out.println(e.getMessage());
        } catch (Exception e) {
            throw new RuntimeException("rethrown", e);
        } finally {
            x = 0;
        }
        Function<Integer, Integer> f = (a) -> a + 1;
        Function<Integer, Integer> g = n -> { return n * 2; };
        Runnable rr = this::run;
        Supplier<List<String>> sup = ArrayList::new;
        nested.forEach(System.out::println);
        int[] arr = new int[]{1, 2, 3};
        arr[0] = arr[1] + arr[2];
    }

    public static void main(String[] args) {
        new KitchenSink().run();
    }
}
