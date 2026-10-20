package t;

// expect: array index is always out of bounds here: the index is 7 and the array length is 3
// expect: array index is always negative here (-1)
// expect: arithmetic overflow: this expression is always outside the range of int here
// expect: arithmetic overflow: this update is always outside the range of int here
// expect: division by zero: the divisor is always 0 here
// expect: negative array size
// expect: array index is always out of bounds here: the index is 10 to 19 and the array length is 4
// expect: infinite recursion
public class ErrRange {
    static int tooFar() {
        int[] a = new int[3];
        int i = 7;
        return a[i];
    }

    static int below() {
        int[] a = new int[3];
        int i = 2;
        i = i - 3;
        return a[i];
    }

    static int big() {
        int x = 2000000000;
        int y = x + x;
        return y;
    }

    static int bump() {
        int x = 2147483647;
        x += 1;
        return x;
    }

    static int zero() {
        int d = 0;
        return 10 / d;
    }

    static int[] neg() {
        int n = 3;
        n = n - 5;
        return new int[n];
    }

    static int loopy() {
        int[] a = new int[4];
        int s = 0;
        for (int i = 10; i < 20; i++) s += a[i];
        return s;
    }

    static int forever(int n) {
        int k = n + 1;
        return forever(k);
    }
}
