package t;

public class Stack4 {
    static void leaf(int n) {
        if (n == 0) throw new IllegalStateException("from the leaf");
        leaf(n - 1);
    }

    static void other() {
        try {
            leaf(2);
        } catch (IllegalArgumentException e) {
            System.out.println("not this one");
        }
    }

    static void middle() {
        try {
            other();
        } finally {
            System.out.println("cleanup");
        }
    }

    public static void main(String[] args) {
        middle();
    }
}
