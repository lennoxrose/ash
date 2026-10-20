package t;

// System.out.print writes without a newline; flush() is allowed (ashvm buffers, kiln does not).
public class Main {
    static void row(int n) {
        for (int i = 0; i < n; i++) {
            System.out.print(i);
            if (i < n - 1) System.out.print(", ");
        }
        System.out.println();
    }

    public static void main(String[] args) {
        System.out.print("a");
        System.out.print(1);
        System.out.print(2L);
        System.out.print(1.5);
        System.out.print(true);
        System.out.print('c');
        String? n = null;
        System.out.print(n);
        Object o = "obj";
        System.out.print(o);
        System.out.flush();
        System.out.println();
        System.err.print("err");
        System.out.println("|");
        row(4);
        row(0);
        System.out.print("");
        System.out.print("no newline at the end of this line, then ");
        System.out.println("one");
        System.out.print("last");
    }
}
