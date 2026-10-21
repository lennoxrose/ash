/// Recursion does not hide a real store: 'keep' stores its parameter, so passing the array moves it.
public class OwnRecursionKeeps {
    static int[] saved;
    static void keep(int[] xs, int n) {
        if (n == 0) { saved = xs; return; }
        keep(xs, n - 1);
    }

    public static void main(String[] args) {
        int[] b = {9};
        keep(b, 1);
        System.out.println(b[0]);
    }
}
