public class OwnFreeFlow {
    static int keep(String s) { return s.length(); }

    static String build(int n) {
        String acc = "";
        for (int i = 0; i < n; i++) {
            acc += i;                         // sole owner, reassigned with a fresh value each time
            if (acc.length() > 6) break;      // break frees acc's loop-scoped siblings only
        }
        String out = acc + "|" + n;           // fresh; escapes through return below, so it is never freed
        return out;
    }

    static int early(int n) {
        String a = "x" + n;
        int[] xs = {n, n + 1, n + 2};
        if (n > 5) return xs[1] + a.length(); // return frees a and xs after computing the value
        String b = a + a;
        return b.length() + xs.length;
    }

    public static void main(String[] args) {
        System.out.println(build(3) + " " + build(9));
        System.out.println(early(2) + " " + early(7));
        String shared = "s" + args.length;
        int k = keep(&shared) + keep(&shared);  // lent to a call: not a sole-owner use, never freed
        String again = shared;                // moved
        Runnable r = () -> System.out.println(again + k);
        r.run();
        for (int i = 0; i < 3; i++) {
            String line = "line " + i;
            String twice = line + line;
            int[] box = new int[2];
            box[1] = twice.length();
            while (true) {
                String inner = twice + box[1];
                System.out.println(inner);
                break;                        // break frees inner only, not line/twice/box
            }
            if (i == 1) continue;
            System.out.println(line + box.length);
        }
    }
}
