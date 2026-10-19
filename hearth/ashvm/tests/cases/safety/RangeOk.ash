public class RangeOk {
    static int sum(int[] a) {
        int s = 0;
        for (int i = 0; i < a.length; i++) s += a[i];
        return s;
    }

    static int countdown() {
        int[] a = new int[5];
        int s = 0;
        int j = 4;
        while (j >= 0) { a[j] = j * j; s += a[j]; j--; }
        return s;
    }

    static int dead() {
        int[] a = new int[2];
        int k = 9;
        if (k < 3) return a[k];       // cannot run: no error
        return 0;
    }

    public static void main(String[] args) {
        int[] xs = {1, 2, 3, 4};
        System.out.println(sum(xs));
        System.out.println(countdown());
        System.out.println(dead());
        int[] a = new int[3];
        int i = 5;
        try {
            System.out.println(a[i]);          // provoked on purpose: a catch is in charge
        } catch (Exception e) {
            System.out.println("caught");
        }
        int d = 0;
        try {
            System.out.println(10 / d);
        } catch (Exception e) {
            System.out.println("caught zero");
        }
        long big = 2000000000L;
        System.out.println(big + big);
    }
}
