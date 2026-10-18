public class ConstOverflow {
    static final int BIG = 2000000000;
    public static void main(String[] args) {
        int a = Integer.MAX_VALUE + 1;
        final int n = 100000;
        int b = n * n;
        int c = BIG + BIG;
        int d = Integer.MIN_VALUE - 1;
        int ok = BIG - BIG + n * 2;
        long fine = Integer.MAX_VALUE + 1L;
        System.out.println(a + b + c + d + ok + fine);
    }
}
