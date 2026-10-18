package t;

import ash.lang.function.Function;

public sealed interface Shape permits Circle, Square {
    double area();

    default String describe() {
        return "shape " + area();
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

abstract class Animal {
    protected final String name;

    protected Animal(String name) {
        this.name = name;
    }

    public abstract String speak();

    @Override
    public String toString() {
        return name + ":" + speak();
    }
}

final class Dog extends Animal {
    Dog() {
        super("dog");
    }

    @Override
    public String speak() {
        return "woof";
    }
}

public class OkOop {
    static String name(Shape s) {
        return switch (s) {
            case Circle c -> "circle " + c.radius();
            case Square q -> "square " + q.side();
        };
    }

    static String label(Level l) {
        return switch (l) {
            case LOW -> "low";
            case HIGH -> "high";
        };
    }

    static int parse(String s) {
        try {
            return Integer.parse(s);
        } catch (ParseException e) {
            return -1;
        } finally {
            System.out.println("parsed");
        }
    }

    static void risky() throws IOException {
        throw new IOException("boom");
    }

    public static void main(String[] args) {
        Shape sh = new Circle(1.5);
        System.out.println(name(sh) + label(Level.HIGH) + new Dog());
        Function<Integer, Integer> inc = a -> a + 1;
        System.out.println(inc.apply(2));
        try {
            risky();
        } catch (IOException e) {
            System.out.println(e.getMessage());
        }
    }
}
