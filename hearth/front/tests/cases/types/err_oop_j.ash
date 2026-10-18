package t;

// expect: must implement abstract method 'speak'
// expect: does not override
// expect: cannot assign a value to final
// expect: is abstract; it cannot be instantiated
abstract class Base {
    abstract String speak();
}

class Impl extends Base { }

class Bad {
    private final int x = 1;

    @Override
    public String nothing() { return "x"; }

    void f() {
        this.x = 2;
        Base b = new Base();
    }
}
