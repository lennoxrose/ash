// for-each loops over arrays and ArrayLists whose bodies keep nothing past the iteration (jown/region.c): every
// loop below must be a region, and must print what ashvm prints even with the reclaimed bytes overwritten.
import ash.util.ArrayList;
import ash.util.List;
import ash.util.Map;
import ash.util.HashMap;

public class RegionForEach {
    static String[] words = {"alpha", "beta", "gamma", "delta", "epsilon"};
    static int[][] grid = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    static List<String> shared = new ArrayList<>();
    int[] cells = {3, 1, 4, 1, 5};

    static long overStrings() {
        long total = 0;
        for (String w : words) {
            String t = w + "-" + w.length();
            String u = t.substring(1) + "!";
            total += u.length() + t.indexOf("-");
            if (w.startsWith("g")) System.out.println(t + "|" + u);
        }
        return total;
    }

    static long overInts(int[] xs) {
        long total = 0;
        for (int x : xs) {
            int[] tmp = new int[4];
            tmp[1] = x * 2;
            String s = "v" + x + "/" + tmp[1];
            total += s.length() + tmp[1];
        }
        return total;
    }

    static long overRows() {
        long total = 0;
        for (int[] row : grid) {
            int[] copy = new int[row.length];
            for (int j = 0; j < row.length; j++) copy[j] = row[j] * 10;
            String line = "row" + copy[0] + "," + copy[1] + "," + copy[2];
            row[0] += 1;                       // a number into an old array: nothing kept
            total += line.length() + copy[2];
        }
        return total;
    }

    static long overList() {
        for (int i = 0; i < 6; i++) shared.add("item" + i);
        long total = 0;
        for (String s : shared) {
            String t = s + s;
            total += t.length() + t.indexOf("m");
        }
        return total;
    }

    static long overBoxed() {
        List<Integer> nums = new ArrayList<>();
        for (int i = 0; i < 8; i++) nums.add(i * i);
        long total = 0;
        for (int n : nums) {
            String s = "n=" + n;
            total += s.length() + n;
        }
        return total;
    }

    static long iterableFromCall() {
        long total = 0;
        for (String p : "a,b,c,dd".split(",")) {
            String q = p + p + p;
            total += q.length();
        }
        return total;
    }

    static long reassignLoopVar() {
        long total = 0;
        for (String w : words) {
            w = w + "?";
            w = w + w;
            total += w.length();
        }
        return total;
    }

    static long overMapKeys() {
        Map<String, Integer> m = new HashMap<>();
        m.put("one", 1);
        m.put("two", 2);
        int total = 0;
        for (String k : m.keys()) {
            String t = k + k;
            total += t.length();
        }
        return total;
    }

    static long nestedForEach() {
        long total = 0;
        for (int i = 0; i < 4; i++) {
            String tag = "t" + i;
            for (String w : words) {
                String both = tag + w;
                total += both.length();
            }
            for (int[] row : grid) {
                for (int c : row) {
                    String s = tag + c;
                    total += s.length();
                }
            }
        }
        return total;
    }

    long fieldArray() {
        long total = 0;
        for (int c : cells) {
            String s = "c" + c;
            cells[0] += 1;
            total += s.length() + c;
        }
        return total;
    }

    static long earlyExit() {
        long total = 0;
        for (String w : words) {
            String t = w + w;
            if (t.length() > 8) continue;
            if (w.equals("delta")) break;
            total += t.length();
        }
        for (String w : words) {
            String t = "x" + w;
            if (w.equals("gamma")) return total + t.length();
        }
        return total;
    }

    static long afterLoops() {
        long total = overStrings();
        String keep = "keep-" + total;
        int[] more = new int[4];
        for (String w : words) {
            String waste = w + keep;
            more[w.length() % 4] += waste.length();
        }
        String after = keep + "-" + more[0] + more[1] + more[2] + more[3];
        return after.length() + more[1];
    }

    public static void main(String[] args) {
        System.out.println(overStrings());
        System.out.println(overInts(new int[]{5, 6, 7, 8}));
        System.out.println(overRows());
        System.out.println(overList());
        System.out.println(overBoxed());
        System.out.println(iterableFromCall());
        System.out.println(reassignLoopVar());
        System.out.println(overMapKeys());
        System.out.println(nestedForEach());
        System.out.println(new RegionForEach().fieldArray());
        System.out.println(earlyExit());
        System.out.println(afterLoops());
        System.out.println(grid[0][0] + grid[1][0] + grid[2][0]);
    }
}
