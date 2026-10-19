package t;

class Boom extends RuntimeException {
    Boom(String m, Throwable cause) { super(m, cause); }
}

public class Stack2 {
    static void leaf(int n) {
        if (n == 0) throw new IllegalStateException("boom");
        leaf(n - 1);
    }

    static void middle() {
        try {
            leaf(2);
        } finally {
            System.out.println("cleanup in middle");
        }
    }

    static void guarded() {
        try {
            middle();
        } catch (IllegalArgumentException e) {
            System.out.println("not this one");
        }
    }

    static void wrap() {
        try {
            guarded();
        } catch (IllegalStateException e) {
            System.out.println("caught " + e.getMessage());
            throw new Boom("wrapped", e);
        }
    }

    public static void main(String[] args) {
        wrap();
    }
}
