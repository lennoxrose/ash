package t;

import ash.util.ArrayList;
import ash.util.List;

// A for-each keeps walking the array / list it began with when its body stores another one in the same variable
// (Java: the old one). The old value used to be freed under the loop, because the variable is a sole owner and a
// reassignment frees what it held: sums came out wrong in ashvm and read freed memory in kiln.
public class RunForeachReassign {
    static int sumList() {
        List<Integer> a = new ArrayList<>();
        for (int i = 0; i < 6; i++) a.add(i + 1);
        int sum = 0;
        for (int x : a) {
            if (x == 2) a = new ArrayList<>();
            sum += x;
            List<Integer> junk = new ArrayList<>();
            junk.add(x * 100);
            junk.add(x * 101);
            sum += junk.get(0) - junk.get(1) + x;
        }
        return sum * 10 + a.size();
    }

    static int sumArray() {
        int[] arr = new int[5];
        for (int i = 0; i < arr.length; i++) arr[i] = i;
        int t = 0;
        for (int v : arr) {
            if (v == 1) arr = new int[3];
            int[] tmp = new int[5];
            tmp[2] = 7 * v;
            t += v + tmp[2];
        }
        return t * 10 + arr.length;
    }

    static int words(int n) {
        String[] ws = new String[4];
        for (int i = 0; i < ws.length; i++) ws[i] = "w" + i + n;
        int len = 0;
        for (String w : ws) {
            ws = new String[1];
            ws[0] = "fresh" + n;
            String t = w + "!" + n;
            len += t.length();
        }
        return len * 10 + ws[0].length();
    }

    static int nested() {
        List<Integer> a = new ArrayList<>();
        for (int i = 1; i <= 3; i++) a.add(i);
        int total = 0;
        for (int x : a) {
            for (int y : a) {
                if (x == 1 && y == 2) a = new ArrayList<>();
                List<Integer> junk = new ArrayList<>();
                junk.add(y);
                total += junk.get(0) + x * 10;
            }
        }
        return total * 10 + a.size();
    }

    public static void main(String[] args) {
        System.out.println(sumList());
        System.out.println(sumArray());
        System.out.println(words(7));
        System.out.println(nested());
    }
}
