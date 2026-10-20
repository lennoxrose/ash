// Loops that look like RegionLoops but let something allocated in the body outlive the iteration. None may be a
// region (region_check.sh counts them): rewinding the heap would corrupt the kept values.
import ash.util.ArrayList;
import ash.util.List;
import ash.lang.function.Function;

public class RegionEscapes {
    static String[] names = new String[4];
    static String latest = "";
    static List<String> shared = new ArrayList<>();
    String field = "";
    int[] buf = new int[3];

    static class Box {
        String text;
        Box(String text) { this.text = text; }
        String get() { return text; }
    }

    static void keep(String s) {
        latest = s;
    }

    static String outerVariable(int n) {
        String last = "";
        for (int i = 0; i < n; i++) {
            last = "value-" + i;
        }
        return last;
    }

    static String outerAppend(int n) {
        String acc = "";
        for (int i = 0; i < n; i++) {
            acc += "x" + i;
        }
        return acc;
    }

    static void outerArray(int n) {
        for (int i = 0; i < n; i++) {
            names[i] = "name-" + i;
        }
    }

    static void staticField(int n) {
        for (int i = 0; i < n; i++) {
            latest = "latest-" + i;
        }
    }

    static void instanceField(RegionEscapes r, int n) {
        for (int i = 0; i < n; i++) {
            r.field = "field-" + i;
        }
    }

    static void outerList(int n) {
        for (int i = 0; i < n; i++) {
            shared.add("shared-" + i);
        }
    }

    static void keepsArgument(int n) {
        for (int i = 0; i < n; i++) {
            keep("kept-" + i);
        }
    }

    static String projectCall(int n) {
        String s = "";
        for (int i = 0; i < n; i++) {
            Box b = new Box("box-" + i);
            s = b.get();
        }
        return s;
    }

    static int[] cells = new int[5];

    static void aliasOuter(int n) {
        for (int i = 0; i < n; i++) {
            int[] alias = cells;
            alias[i] = i * 10;
            int[] fresh = new int[2];
            fresh[0] = i;
        }
    }

    static void rowOfOuter(int[][] g, int n) {
        for (int i = 0; i < n; i++) {
            int[] row = new int[2];
            row[0] = i;
            g[i] = row;
        }
    }

    static String lambdaCapture(int n) {
        String r = "";
        for (int i = 0; i < n; i++) {
            String s = "cap-" + i;
            Function<String, String> f = x -> x + s;
            r = f.apply("<");
        }
        return r;
    }

    static String? throwsOut(int n) {
        try {
            for (int i = 0; i < n; i++) {
                String s = "t-" + i;
                if (i == n - 1) throw new RuntimeException(s);
            }
        } catch (RuntimeException e) {
            return e.getMessage();
        }
        return "";
    }

    static String forEachOuter(String[] in) {
        String last = "";
        for (String s : in) {
            String t = s + "!";
            last = t;
        }
        return last;
    }

    static String switchKeeps(int n) {
        String last = "";
        for (int i = 0; i < n; i++) {
            switch (i % 2) {
                case 0: last = "even-" + i; break;
                default: last = "odd-" + i;
            }
        }
        return last;
    }

    static String stringBuilder(int n) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < n; i++) {
            sb.append("sb-").append(i);
        }
        return sb.toString();
    }

    public static void main(String[] args) {
        System.out.println(outerVariable(6));
        System.out.println(outerAppend(4));
        outerArray(4);
        System.out.println(names[0] + names[1] + names[2] + names[3]);
        staticField(5);
        System.out.println(latest);
        RegionEscapes r = new RegionEscapes();
        instanceField(r, 5);
        System.out.println(r.field);
        outerList(4);
        System.out.println(shared.get(0) + shared.get(3));
        keepsArgument(3);
        System.out.println(latest);
        System.out.println(projectCall(4));
        aliasOuter(5);
        System.out.println(cells[0] + cells[1] + cells[2] + cells[3] + cells[4]);
        int[][] g = new int[3][];
        rowOfOuter(g, 3);
        System.out.println(g[0][0] + g[1][0] + g[2][0]);
        System.out.println(lambdaCapture(3));
        System.out.println(throwsOut(4));
        String[] words = {"a", "bb", "ccc"};
        System.out.println(forEachOuter(words));
        System.out.println(switchKeeps(5));
        System.out.println(stringBuilder(4));
    }
}
