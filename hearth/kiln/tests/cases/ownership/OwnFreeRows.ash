public class OwnFreeRows {
    private int[][] grid = new int[6][8];             // owned field: its rows die with the object
    static int[] kept;                                // a row that escapes
    static int[][] cache;

    int touch(int k) {
        grid[1][2] = k;
        return grid[1][2] + grid.length + grid[0].length;
    }

    static int sum(int[][] m) {
        int s = 0;
        for (int i = 0; i < m.length; i++) for (int j = 0; j < m[i].length; j++) s += m[i][j];
        return s;
    }

    static class Board {
        private int[][] cells = new int[5][5];        // never leaves the object: its rows are freed with it
        int play(int k) { cells[2][3] = k; return cells[2][3] + cells.length; }
    }

    static String first(int i) {
        String[] xs = new String[2];
        xs[0] = "a" + i;
        xs[1] = "b" + i;
        return xs[0];                                 // an element leaves with the return
    }

    static void stash(int[][] m) { kept = m[1]; }     // takes a row out of a parameter: callers must not free the rows
    static int[] rowOf(int[][] m, int i) { return m[i]; }

    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 20000; i++) {
            int[][] a = new int[8][16];               // rows freed with the array
            a[3][4] = i;
            total += a[3][4] + a.length + a[0].length + sum(a);

            int[][] b = new int[4][4];                // a row escapes through a local alias
            int[] row = b[2];
            row[1] = i;
            if (i == 7) kept = row;
            total += b[2][1];

            int[][] c = new int[3][3];                // the callee keeps a row
            c[1][1] = i;
            if (i == 9) stash(c);
            total += sum(c);

            int[][] d = new int[3][3];                // rows handed out by a helper
            d[0][0] = i;
            int[] r = rowOf(d, 0);
            if (i == 11) kept = r;

            int[][] e = new int[2][5];                // for-each variable over the rows
            for (int[] x : e) x[0] = i;
            int[] last = e[0];
            for (int[] x : e) last = x;
            if (i == 13) kept = last;

            int[][] f = new int[2][2];                // a shared row stored in
            int[] shared = new int[2];
            shared[0] = i;
            total += shared[0];
            f[0] = shared;
            total += f[0][0];

            int[][] g = new int[3][];                 // rows made one by one
            g[0] = new int[2];
            g[1] = new int[3];
            g[1][2] = i;
            total += g[1][2] + g[1].length;

            int[][][] h = new int[2][3][4];           // three sizes: left to the runtime
            h[1][2][3] = i;
            total += h[1][2][3];

            OwnFreeRows o = new OwnFreeRows();
            total += o.touch(i);
            if (i == 15) cache = o.grid;              // the field leaves the object
            Board bd = new Board();
            total += bd.play(i);
            total += first(i).length();
            int[][] k = new int[2][2];
            k[0][0] = 5;
            if (i == 17) cache = k;                   // the array itself leaves
        }
        System.out.println(total);
        System.out.println(kept[1] + " " + kept.length);
        System.out.println(cache[0][0] + " " + cache.length);
    }
}
