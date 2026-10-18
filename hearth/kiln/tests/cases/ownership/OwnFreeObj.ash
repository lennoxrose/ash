public class OwnFreeObj {
    static class Point {
        int x;
        int y;
        Point(int x, int y) { this.x = x; this.y = y; }
        int sum() { return x + y; }
        void bump() { x = x + 1; }
    }

    static class Leaky {
        int v;
        Leaky(int v) { this.v = v; }
        Leaky self() { return this; }       // `this` escapes: instances are never freed
    }

    static Point kept;

    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 200000; i++) {
            Point p = new Point(i, 2);      // sole owner: freed at the end of each iteration
            p.bump();
            Point q = new Point(1, 1);
            q.x = p.sum();
            if (i % 100000 == 0) {
                System.out.println(q.sum());
                continue;
            }
            Leaky l = new Leaky(i);
            Leaky same = l.self();          // l is not a sole owner
            if (i == 5) kept = new Point(7, 8);
            total += q.sum() + same.v % 3;
        }
        System.out.println(total + " " + kept.sum());
    }
}
