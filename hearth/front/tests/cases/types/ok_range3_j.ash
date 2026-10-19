package t;

// An array chosen by `?:` has the hull of the two lengths; when one arm is unknown nothing is claimed.
public class OkRange3 {
    static int fine(bool flag, int[] p) {
        int[] x = new int[3];
        int[] c = flag ? x : p;
        int[] d = flag ? new int[2] : new int[9];
        int[] e = flag ? new int[4] : new int[6];
        return c[5] + d[1] + e[5];
    }
}
