// Valid programs the range analysis used to reject because a fact outlived the code that changed it: a
// condition that changes a variable it also tests, an element assignment whose right side changes the index
// (Java evaluates the array and the index first), and a finally block that runs between a break and the loop exit.
public class run_range_stale_j {
    static int[] fill(int n) {
        int[] a = new int[n];
        for (int i = 0; i < n; i++) a[i] = 10 + i;
        return a;
    }

    public static void main(String[] args) {
        // && : the right side changes x after the left side was tested
        int x = 4;
        int[] arr = fill(5);
        if (x < 5 && x++ >= 0) {
            System.out.println("and: " + arr[x - 5]);
        }
        int x2 = 0;
        if (x2 == 0 || x2++ > 5) System.out.println("or: " + arr[x2]);
        int x3 = 1;
        if (!(x3 > 3 || (x3 += 3) > 100)) System.out.println("not: " + arr[x3 - 4]);

        // loop conditions that change what they test
        int w = 5;
        while (w < 6 && w++ > 0) { }
        System.out.println("while: " + arr[w - 6]);
        int z = 0;
        do { z++; } while (z < 3 && (z += 10) > 0);
        System.out.println("do: " + arr[z - 12]);

        // the index of the target is evaluated before the right side
        int[] a = fill(3);
        int i = 2;
        a[i] = i++;
        System.out.println("target: " + a[2] + " " + i);
        int j = 4;
        int[] b = fill(5);
        b[j--] = b[j + 1];
        System.out.println("order: " + b[4] + " " + j);
        int[][] g = new int[2][3];
        int r = 1;
        g[r][r + 1] = r--;
        System.out.println("grid: " + g[1][2] + " " + r);

        // break leaves through a finally that assigns
        int f = 0;
        while (true) {
            try {
                break;
            } finally {
                f = 5;
            }
        }
        System.out.println("finally: " + arr[f - 5]);
        int g2 = 0;
        for (int n = 0; n < 3; n++) {
            try {
                if (n == 1) continue;
            } finally {
                g2 += 2;
            }
        }
        System.out.println("continue: " + arr[g2 - 6]);
    }
}
