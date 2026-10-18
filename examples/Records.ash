package examples;

record Point(int x, int y) {
    Point {
        if (x < 0 || y < 0) {
            throw new IllegalArgumentException("negative coordinate");
        }
    }

    Point plus(Point other) {
        return new Point(x + other.x(), y + other.y());
    }

    int manhattan() {
        return x + y;
    }
}

sealed interface Expr permits Num, Add, Mul { }
record Num(int value) implements Expr { }
record Add(Expr left, Expr right) implements Expr { }
record Mul(Expr left, Expr right) implements Expr { }

public class Records {
    static int eval(Expr e) {
        return switch (e) {
            case Num n -> n.value();
            case Add a -> eval(a.left()) + eval(a.right());
            case Mul m -> eval(m.left()) * eval(m.right());
        };
    }

    public static void main(String[] args) {
        Point p = new Point(1, 2).plus(new Point(3, 4));
        System.out.println(p + " " + p.manhattan() + " " + p.equals(new Point(4, 6)));
        try {
            new Point(-1, 0);
        } catch (IllegalArgumentException e) {
            System.out.println("rejected: " + e.getMessage());
        }
        Expr tree = new Add(new Num(2), new Mul(new Num(3), new Num(4)));
        System.out.println(eval(tree));
    }
}
