package t;

// expect: array index is always out of bounds here: the index is 6 and the array length is 4 to 6
// expect: array index is always out of bounds here: the index is 5 and the array length is 3 to 5
public class ErrRange3 {
    static int fresh(bool flag) {
        int[] c = flag ? new int[4] : new int[6];
        return c[6];
    }

    static int aliases(bool flag) {
        int[] x = new int[3];
        int[] y = new int[5];
        int[] c = flag ? x : y;
        return c[5];
    }
}
