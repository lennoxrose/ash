// Loops that call project methods which may keep something they allocate, or whose behaviour is not fixed. None of
// the loops that call them may be a region.
import ash.util.ArrayList;
import ash.util.List;

public class RegionCallEscapes {
    static String latest = "";
    static String[] slots = new String[8];
    static List<String> seen = new ArrayList<>();
    static int[] counts = new int[4];

    static int keepsInStatic(int n) {
        latest = "n" + n;
        return latest.length();
    }

    static int keepsInArray(int n) {
        slots[n % 8] = "s" + n;
        return slots[n % 8].length();
    }

    static int keepsInList(int n) {
        seen.add("v" + n);
        return seen.size();
    }

    static String returnsString(int n) {
        return "r" + n;
    }

    static int takesArray(int[] a) {
        a[0] += 1;
        return a[0];
    }

    static int recurse(int n) {
        String s = "x" + n;
        if (n <= 0) return s.length();
        return recurse(n - 1) + s.length();
    }

    static int ping(int n) {
        String s = "a" + n;
        return n <= 0 ? 0 : pong(n - 1) + s.length();
    }

    static int pong(int n) {
        String s = "b" + n;
        return n <= 0 ? 0 : ping(n - 1) + s.length();
    }

    static int callsImpure(int n) {
        String s = "c" + n;
        return keepsInStatic(n) + s.length();
    }

    static int throwsInside(int n) {
        String s = "t" + n;
        if (n > 1000) throw new RuntimeException(s);
        return s.length();
    }

    static int triesInside(int n) {
        String s = "y" + n;
        try {
            return s.length();
        } catch (RuntimeException e) {
            return 0;
        }
    }

    static int lambdaInside(int n) {
        String s = "l" + n;
        Runnable r = () -> latest = s;
        r.run();
        return s.length();
    }

    static int makesObject(int n) {
        StringBuilder sb = new StringBuilder();
        sb.append("sb").append(n);
        return sb.length();
    }

    int overridable(int n) {
        String s = "o" + n;
        return s.length();
    }

    static int keepsArgument(String s) {
        int n = s.length();
        latest = s;
        return n;
    }

    public int solo(int n) {
        String s = "solo" + n;
        return s.length();
    }

    static class Sub extends RegionCallEscapes {
        @Override
        int overridable(int n) {
            latest = "sub" + n;
            return 1;
        }
    }

    static long loop1(int n) { long t = 0; for (int i = 0; i < n; i++) t += keepsInStatic(i); return t; }
    static long loop2(int n) { long t = 0; for (int i = 0; i < n; i++) t += keepsInArray(i); return t; }
    static long loop3(int n) { long t = 0; for (int i = 0; i < n; i++) t += keepsInList(i); return t; }
    static long loop4(int n) { long t = 0; for (int i = 0; i < n; i++) t += returnsString(i).length(); return t; }
    static long loop5(int n) { long t = 0; for (int i = 0; i < n; i++) { int[] a = new int[2]; t += takesArray(a); } return t; }
    static long loop6(int n) { long t = 0; for (int i = 0; i < n; i++) t += recurse(3); return t; }
    static long loop7(int n) { long t = 0; for (int i = 0; i < n; i++) t += ping(4); return t; }
    static long loop8(int n) { long t = 0; for (int i = 0; i < n; i++) t += callsImpure(i); return t; }
    static long loop9(int n) { long t = 0; for (int i = 0; i < n; i++) t += throwsInside(i); return t; }
    static long loop10(int n) { long t = 0; for (int i = 0; i < n; i++) t += triesInside(i); return t; }
    static long loop11(int n) { long t = 0; for (int i = 0; i < n; i++) t += lambdaInside(i); return t; }
    static long loop12(int n) { long t = 0; for (int i = 0; i < n; i++) t += makesObject(i); return t; }
    static long loop13(RegionCallEscapes o, int n) { long t = 0; for (int i = 0; i < n; i++) t += o.overridable(i); return t; }

    static long loop14(int n) { long t = 0; for (int i = 0; i < n; i++) t += keepsArgument("arg" + i); return t; }
    static long loop15(RegionCallEscapes o, int n) { long t = 0; for (int i = 0; i < n; i++) t += o.solo(i); return t; }

    public static void main(String[] args) {
        System.out.println(loop1(5) + latest);
        System.out.println(loop2(10) + slots[1] + slots[7]);
        System.out.println(loop3(5) + seen.get(4));
        System.out.println(loop4(5));
        System.out.println(loop5(5));
        System.out.println(loop6(3));
        System.out.println(loop7(3));
        System.out.println(loop8(4) + latest);
        System.out.println(loop9(4));
        System.out.println(loop10(4));
        System.out.println(loop11(4) + latest);
        System.out.println(loop12(4));
        System.out.println(loop13(new Sub(), 3) + latest);
        System.out.println(loop13(new RegionCallEscapes(), 3));
        System.out.println(loop14(4) + latest);
        System.out.println(loop15(new RegionCallEscapes(), 4));
    }
}
