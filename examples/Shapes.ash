package examples;

interface Shape {
    double area();

    default String describe() {
        return "shape with area " + area();
    }
}

abstract class Polygon implements Shape {
    protected final String label;

    protected Polygon(String label) {
        this.label = label;
    }

    public abstract int sides();

    @Override
    public String describe() {
        return label + " (" + sides() + " sides), area " + area();
    }
}

class Rect extends Polygon {
    private final double w;
    private final double h;

    Rect(double w, double h) {
        super("rect");
        this.w = w;
        this.h = h;
    }

    Rect(String label, double w, double h) {
        super(label);
        this.w = w;
        this.h = h;
    }

    public double area() { return w * h; }
    public int sides() { return 4; }
}

class Square extends Rect {
    Square(double side) {
        super("square", side, side);
    }

    @Override
    public String describe() {
        return "!" + super.describe();
    }
}

class Disc implements Shape {
    private final double r;

    Disc(double r) { this.r = r; }

    public double area() { return 3.0 * r * r; }
}

public class Shapes {
    public static void main(String[] args) {
        Shape[] shapes = {new Rect(2.0, 3.5), new Square(4.0), new Disc(2.0)};
        for (Shape s : shapes) {
            System.out.println(s.describe());
        }
    }
}
