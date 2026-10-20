public class RangeErrors {
    static int tooFar() {
        int[] a = new int[3];
        int i = 7;
        return a[i];
    }

    static int loopPastEnd() {
        int[] a = new int[4];
        int s = 0;
        for (int i = 10; i < 20; i++) s += a[i];
        return s;
    }

    static int overflow() {
        int x = 2000000000;
        return x + x;
    }

    static int zero() {
        int d = 0;
        return 10 / d;
    }

    static int forever(int n) {
        return forever(n + 1);
    }

    public static void main(String[] args) {
        System.out.println(tooFar() + loopPastEnd() + overflow() + zero() + forever(0));
    }
}
