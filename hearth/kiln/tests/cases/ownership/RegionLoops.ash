// Loops whose body provably keeps nothing it allocates past the iteration: kiln rewinds the heap after every
// iteration (jown/region.c). Everything here must print exactly what ashvm prints; region_check.sh also runs
// this file with the reclaimed memory overwritten (KILN_REGION_POISON) and expects every loop below to be a region.
import ash.util.ArrayList;
import ash.util.List;

public class RegionLoops {
    static int counter;
    private int hits;

    static long strings(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            String s = "item-" + i;
            String t = s + "!" + (i * 2) + 'c' + (i % 2 == 0) + 2;
            total += t.length() + s.indexOf("-") + s.substring(2).length();
            if (t.startsWith("item-1")) total += 1000;
            if (s.equals("item-3")) System.out.println("found " + s + " / " + t);
        }
        return total;
    }

    static long arrays(int n) {
        long total = 0;
        int i = 0;
        while (i < n) {
            int[] xs = new int[8];
            double[] ds = {i, i + 0.5, i * 2.0};
            String[] names = new String[3];
            names[0] = "a" + i;
            names[1] = names[0] + "b";
            names[2] = names[1] + names[0];
            xs[3] = i;
            xs[4] = xs[3] * 2;
            int[][] grid = new int[3][4];
            grid[1][2] = i;
            total += xs[4] + xs.length + (long) ds[2] + names[2].length() + grid[1][2];
            i++;
        }
        return total;
    }

    static long lists(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            List<String> l = new ArrayList<>();
            for (int j = 0; j < 5; j++) l.add("v" + i + "-" + j);
            l.set(2, "two");
            String removed = l.remove(1);
            l.add(0, "first");
            total += l.size() + l.get(2).length() + removed.length();
            ArrayList<Integer> nums = new ArrayList<>();
            nums.add(i);
            nums.add(i + 1);
            total += nums.get(1);
            if (!nums.isEmpty()) nums.removeLast();
            total += nums.size();
        }
        return total;
    }

    static long nested(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            String row = "r" + i;
            for (int j = 0; j < 3; j++) {
                String cell = row + "c" + j;
                if (j == 1) continue;
                total += cell.length();
            }
            String acc = "";
            int k = 0;
            while (k < 4) {
                acc += k;
                acc = acc + "," + row;
                k++;
            }
            total += acc.length();
        }
        return total;
    }

    static long counters(int n) {
        long total = 0;
        RegionLoops self = new RegionLoops();
        for (int i = 0; i < n; i++) {
            String s = "x" + i;
            counter += s.length();
            self.hits += 2;
            total += counter + self.hits;
        }
        return total;
    }

    static int earlyReturn(int n) {
        for (int i = 0; i < n; i++) {
            String s = "round-" + i;
            int[] a = new int[4];
            a[1] = i;
            if (i == n - 2) return s.length() + a[1];
        }
        return -1;
    }

    static String earlyReturnString(int n) {
        for (int i = 0; i < n; i++) {
            String s = "round-" + i + "-" + n;
            if (i == n - 1) return s;
            String waste = s + s;
            if (waste.length() < 0) break;
        }
        return "none";
    }

    static long breaks(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            String s = "b" + i;
            if (i > 3) break;
            if (i == 2) continue;
            total += s.length();
        }
        return total;
    }

    static long mathAndPrint(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            double d = Math.sqrt(i * 4.0);
            int a = Math.abs(i - 3);
            String line = "d=" + (int) d + " a=" + a;
            total += line.length();
            if (i % 3 == 0) System.out.println(line);
            if (i == 4) System.out.println((int) d);
            System.out.println(i);
        }
        return total;
    }

    static long everyOperation(int n) {
        long total = 0;
        for (int i = 0; i < n; i++) {
            long big = 5000000000L + i;
            char c = (char) ('a' + i % 26);
            bool flag = i % 3 == 0;
            String text = "n=" + i + " big=" + big + " c=" + c + " f=" + flag + " d=" + (i + 0.5);
            String rest = text.substring(text.indexOf("big"));
            if (rest.startsWith("big=")) total += 1;
            if (rest.endsWith("" + c + " f=" + flag + " d=" + (i + 0.5))) total += 2;
            if (text.contains("c=" + c)) total += 4;
            if (!text.isEmpty() && text.compareTo(rest) != 0) total += 8;
            if (text.equals(rest)) total += 16;
            total += Math.max(i, 3) + Math.min(i, 3) + (long) Math.floor(i / 2.0) + (long) Math.abs(i - 5.5) + text.charAt(1);
            String shown = "" + big + c + flag + i;
            total += shown.length();
            if (i % 5 == 0) System.out.println(text + "|" + rest + "|" + shown);
        }
        return total;
    }

    static long afterMany(int n) {
        // a region loop followed by ordinary allocation: values made after the loop must be intact
        long total = strings(n);
        String keep = "keep-" + total;
        int[] more = new int[5];
        for (int i = 0; i < n; i++) {
            String waste = "w" + i + keep;
            more[i % 5] += waste.length();
        }
        String after = keep + "-" + more[0] + more[1] + more[2];
        return after.length() + more[3] + more[4];
    }

    public static void main(String[] args) {
        System.out.println(strings(50));
        System.out.println(arrays(30));
        System.out.println(lists(20));
        System.out.println(nested(10));
        System.out.println(counters(7));
        System.out.println(earlyReturn(10));
        System.out.println(earlyReturnString(6));
        System.out.println(breaks(10));
        System.out.println(mathAndPrint(7));
        System.out.println(everyOperation(12));
        System.out.println(afterMany(40));
    }
}
