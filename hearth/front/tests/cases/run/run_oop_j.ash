package t;

interface Greeter {
    String name();

    default String greet() {
        return "hello " + name();
    }
}

abstract class Base implements Greeter {
    protected int calls = 0;

    public abstract String kind();

    public String describe() {
        calls++;
        return kind() + "/" + name() + "/" + calls;
    }
}

class English extends Base {
    public String name() { return "en"; }
    public String kind() { return "english"; }
}

class Polite extends English {
    @Override
    public String name() { return "polite"; }

    @Override
    public String greet() { return "good day, " + name(); }
}

class Counter {
    private static int created = 0;
    private final int id;

    Counter() {
        created++;
        this.id = created;
    }

    static int total() { return created; }
    int getId() { return id; }
}

public class RunOop {
    public static void main(String[] args) {
        Base a = new English();
        Base b = new Polite();
        System.out.println(a.describe());
        System.out.println(a.describe());
        System.out.println(b.describe());
        System.out.println(a.greet());
        System.out.println(b.greet());
        Greeter g = b;
        System.out.println(g.name());
        new Counter();
        Counter c = new Counter();
        System.out.println(c.getId() + " of " + Counter.total());
        Object o = a;
        System.out.println(o instanceof Greeter);
        System.out.println(o instanceof Polite);
        System.out.println(b instanceof English e ? e.kind() : "no");
    }
}
