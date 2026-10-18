public class OwnBorrow {
    static int size(String s) { return s.length(); }
    static void append(int[] xs, int[] ys) { xs[0] = ys[0]; }

    public static void main(String[] args) {
        String a = "abc";
        int n = size(&a) + size(&a);      // shared borrows may overlap
        int[] xs = new int[2];
        int[] ys = new int[2];
        append(&mut xs, &ys);             // different variables: fine
        append(&mut xs, &mut ys);
        System.out.println(n + a + xs.length);
    }
}
