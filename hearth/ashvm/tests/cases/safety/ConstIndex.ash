public class ConstIndex {
    public static void main(String[] args) {
        final int[] xs = new int[3];
        int[] lit = {1, 2};
        int a = xs[3];                    // past the end of a final array
        int b = xs[-1];                   // negative
        int c = 0;
        int d = xs[2];
        System.out.println(a + b + c + d);
    }
}
