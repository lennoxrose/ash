import ash.util.ArrayList;
import ash.util.List;

public class OwnBorrowTricky {
    static class Animal {
        String name = "animal";
        String sound() { return "..."; }
    }
    static class Dog extends Animal {
        static String last;
        String sound() { last = name; return "woof"; }     // an override that keeps a field: parameters of type Animal may be Dogs
    }
    static class Point {
        int x;
        int y;
        Point(int x, int y) { this.x = x; this.y = y; }
    }

    static List<String> sink = new ArrayList<>();

    static int reads(String s) { return s.length(); }
    static void keeps(String s) { sink.add(s); }                  // passes it to library code that keeps it
    static int lam(String s) {
        Runnable r = () -> sink.add(s);                           // captured by a lambda
        r.run();
        return 1;
    }
    static int chain(String s, int depth) {
        if (depth == 0) return s.length();
        return chain(s, depth - 1);                               // recursion: treated as escaping, never freed by the caller
    }
    static int pointSum(Point p) { return p.x + p.y; }            // reads fields only: a plain class with no subclass is fine
    static String call(Animal a) { return a.sound(); }            // may run Dog.sound: never borrowed
    static int overwrite(String s) {
        s = s + "!";                                              // the parameter is a copy: the caller's value is untouched
        return s.length();
    }

    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 2000; i++) {
            String a = "a" + i;
            String b = "b" + i;
            String c = "c" + i;
            String d = "d" + i;
            String e = "e" + i;
            Point p = new Point(i, 1);
            total += reads(a) + reads(a) + overwrite(b) + chain(e, 3) + pointSum(p);
            keeps(c);                                             // c now lives in sink
            total += lam(d);                                      // d now lives in sink
            total += reads(b) + a.length();
        }
        Animal pet = new Dog();
        System.out.println(total + " " + sink.size() + " " + sink.get(1999) + " " + call(pet) + " " + Dog.last);
    }
}
