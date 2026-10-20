package t;

interface Sink { void take(int[] xs); }

class Box {
    int[] held;
    void keep(int[] xs) { held = xs; }
}

public class Main {
    static int[] keep;
    static void store(int[] p) { keep = p; }

    static void viaCall() {
        int[] a = {1, 2, 3};
        store(a);
        System.out.println(a[0]);                 // moved into a call whose target keeps it
    }

    static void viaOverridable(Box b) {
        int[] a = {1, 2, 3};
        b.keep(a);
        System.out.println(a[0]);                 // Box.keep keeps it, but the target is fixed here
    }

    static void viaInterface(Sink s) {
        int[] a = {1, 2, 3};
        s.take(a);
        System.out.println(a[0]);                 // the target is not fixed: a loan would do
    }

    static void viaAssign() {
        int[] a = {1, 2, 3};
        int[] b = a;
        System.out.println(a[0] + b[0]);          // moved by the assignment
    }

    static void inLoop() {
        int[] a = {1, 2, 3};
        for (int i = 0; i < 2; i++) {
            System.out.println(a[0]);             // moved in the previous iteration
            store(a);
        }
    }

    static void fresh() {
        int[] a = {1, 2, 3};
        store(a);
        a = new int[2];
        System.out.println(a[0]);                 // fine: a new value
    }

    public static void main(String[] args) {
        viaCall();
    }
}
