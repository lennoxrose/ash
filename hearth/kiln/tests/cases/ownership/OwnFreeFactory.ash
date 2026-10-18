public class OwnFreeFactory {
    static class Point {
        int x;
        int y;
        Point(int x, int y) { this.x = x; this.y = y; }
        int sum() { return x + y; }
    }

    static class Shape {
        int w = 3;
        int area() { return w * w; }
    }

    static class Square extends Shape {
        int area() { return w * w * 2; }              // Shape has a subclass: its instances are never assumed exact
    }

    static Point make(int i) {
        Point p = new Point(i, 1);                    // sole owner, handed to the caller
        p.x = p.x + 1;
        return p;
    }

    static Point origin() { return new Point(0, 0); }

    static Shape shape(int i) {
        if (i % 2 == 0) return new Shape();
        return new Square();                          // two classes: never fresh
    }

    static Point cached = new Point(5, 5);
    static Point shared() {
        return cached;                                // an alias of a static: never fresh
    }

    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 100000; i++) {
            Point a = make(i);                        // freed each iteration
            Point b = origin();                       // freed each iteration
            Shape s = shape(i);
            Point c = shared();                       // alias: must survive
            total += a.sum() + b.sum() + s.area() + c.sum();
        }
        System.out.println(total + " " + shared().sum());
    }
}
