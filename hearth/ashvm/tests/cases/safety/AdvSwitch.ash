// switch fall-through, shared labels, and initialisation across cases: valid programs that must compile.
public class AdvSwitch {
    static int pick(int k) {
        int x;
        switch (k) {
            case 0: x = 1; break;
            case 1: x = 2;
            case 2: x = x2(k); break;
            default: x = 4;
        }
        return x;
    }
    static int x2(int k) { return k * 10; }

    static int narrow(int n, int[] a) {
        int r = 0;
        switch (n) {
            case 1:
                r += a[n];            // n is 1
            case 2:
                r += a[n];            // n is 1 or 2 (fall-through): a has 3 elements
                break;
            case 3:
                r += a[n - 1];
                break;
            default:
                r = -1;
        }
        return r;
    }

    static String arrow(int k) {
        String s;
        switch (k) {
            case 1, 2 -> s = "low";
            case 3 -> s = "three";
            default -> s = "other";
        }
        return s;
    }

    static int maybe(int k) {
        int y;
        switch (k) {
            case 0: y = 1; break;
            case 1: break;
            default: y = 3;
        }
        if (k != 1) return y;          // assigned on every path that gets here, the analysis only needs "some path"
        return 0;
    }

    static int expr(int k) {
        int z = switch (k) {
            case 0 -> 10;
            case 1 -> 5 * 2;
            default -> throw new IllegalArgumentException("k");
        };
        return z;
    }

    public static void main(String[] args) {
        int[] a = {10, 20, 30};
        StringBuilder sb = new StringBuilder();
        for (int k = 0; k < 5; k++) sb.append(pick(k)).append(',');
        sb.append(' ');
        for (int n = 0; n < 5; n++) sb.append(narrow(n, a)).append(',');
        sb.append(' ');
        for (int k = 0; k < 5; k++) sb.append(arrow(k)).append(',');
        sb.append(' ');
        for (int k = 0; k < 4; k++) sb.append(maybe(k)).append(',');
        sb.append(' ');
        sb.append(expr(0)).append(',').append(expr(1));
        try { expr(2); } catch (IllegalArgumentException e) { sb.append(" caught"); }
        System.out.println(sb.toString());
    }
}
