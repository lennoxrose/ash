// Initialisation through every kind of assignment: conditions, chained and conditional assignments, do-while, try /
// finally, switch arms, for headers, instanceof bindings. All valid (and accepted by javac): none may be reported.
public class AdvInit {
    static int next(int[] box) { box[0]++; return box[0] < 4 ? box[0] : -1; }
    interface Get { int get(); }
    public static void main(String[] args) {
        int[] box = new int[1];
        int line;
        int total = 0;
        while ((line = next(box)) > 0) total += line;
        total += line;

        int x;
        do { x = next(box); } while (x > 0);
        total += x;

        int y, z;
        y = z = 5;
        total += y + z;

        int r;
        int w = (args.length == 0) ? (r = 1) : (r = 2);
        total += r + w;

        int t;
        try { t = next(box); } catch (RuntimeException e) { t = 0; }
        total += t;

        int f;
        try { total += 1; } finally { f = 7; }
        total += f;

        int sw;
        switch (args.length) { case 0 -> sw = 1; case 1 -> sw = 2; default -> sw = 3; }
        total += sw;

        int g;
        Get gg = () -> 3;
        g = gg.get();
        total += g;

        int h;
        if (args.length == 0 && (h = 4) > 0) total += 1;

        int k;
        for (k = 0; k < 3; k++) { total += k; }
        total += k;

        int m;
        for (int i = 0; ; i++) { if (i == 2) { m = i; break; } }
        total += m;

        String s;
        Object o = "str";
        if (o instanceof String str) { s = str; } else { s = "none"; }
        total += s.length();

        int n;
        bool ok = args.length == 0;
        if (ok) n = 1; else n = 2;
        total += n;

        int p;
        if (!ok) { p = 1; } else if (total > 0) { p = 2; } else { p = 3; }
        total += p;

        System.out.println(total);
    }
}
