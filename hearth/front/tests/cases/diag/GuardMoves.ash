package t;

public class Main {
    static int[] keep;
    static bool hold(int[] p) { keep = p; return false; }

    static void f(Object o) {
        int[] a = {1, 2, 3};
        switch (o) {
            case Integer i when hold(a):        // moves a, whether or not the guard holds
                System.out.println("held");
                break;
            default:
                System.out.println(a[0]);       // the guard may have run and failed: a is moved
        }
    }

    public static void main(String[] args) {
        f(1);
    }
}
