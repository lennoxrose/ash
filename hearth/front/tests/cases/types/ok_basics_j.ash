package t;

import ash.util.List;
import ash.util.ArrayList;
import ash.util.Map;
import ash.util.HashMap;

public class OkBasics {
    private static final int LIMIT = 5;
    private int count;

    public OkBasics(int count) {
        this.count = count;
    }

    public int getCount() {
        return count;
    }

    static int sum(int[] xs) {
        int total = 0;
        for (int x : xs) {
            total += x;
        }
        return total;
    }

    static long widen(int a, long b) {
        byte small = 5;
        char c = 'x';
        double d = a + b * 2.5;
        float f = 1.5f;
        return a + b + small + c + (long) d + (long) f;
    }

    static String describe(Object o) {
        if (o instanceof String s && s.length() > 0) {
            return s;
        }
        return "other " + o;
    }

    static int len(String? maybe) {
        if (maybe == null) {
            return 0;
        }
        return maybe.length();
    }

    static String firstOr(List<String> xs, String fallback) {
        return xs.isEmpty() ? fallback : xs.get(0);
    }

    static <T> T pick(T a, T b, bool first) {
        return first ? a : b;
    }

    public static void main(String[] args) {
        List<String> names = new ArrayList<>();
        names.add("ash");
        Map<String, Integer> counts = new HashMap<>();
        counts.put("a", 1);
        Integer? got = counts.get("a");
        if (got != null) {
            int v = got;
            System.out.println(v + LIMIT);
        }
        String p = pick("x", "y", true);
        var n = new OkBasics(3).getCount();
        System.out.println(firstOr(names, p) + n + sum(new int[]{1, 2}) + widen(1, 2L));
        System.out.println(len(null) + describe("q"));
        int[][] grid = {{1, 2}, {3, 4}};
        System.out.println(grid[1][0] + grid.length);
    }
}
