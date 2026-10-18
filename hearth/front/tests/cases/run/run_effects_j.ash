package t;

import ash.util.ArrayList;
import ash.util.List;

public class RunEffects {
    static int calls = 0;

    static int next() {
        calls++;
        return calls;
    }

    public static void main(String[] args) {
        int i = 0;
        int a = i++ + i++;
        System.out.println(a + " " + i);
        int b = ++i * 2;
        System.out.println(b + " " + i);
        int x;
        int y;
        x = y = 5;
        System.out.println(x + " " + y);
        int[] arr = new int[4];
        int k = 0;
        arr[k++] = 10;
        arr[k++] = 20;
        arr[k] = k++;
        System.out.println(arr[0] + " " + arr[1] + " " + arr[2] + " " + k);
        int z = 3;
        int w = z + (z = 10);
        System.out.println(w + " " + z);
        int c = 0;
        bool hit = c > 0 && (c = 99) > 0;
        bool hit2 = c == 0 || (c = 98) > 0;
        System.out.println(hit + " " + hit2 + " " + c);
        bool t1 = c == 0 && (c = 7) == 7;
        System.out.println(t1 + " " + c);
        int tern = c > 5 ? (c = 1) : (c = 2);
        System.out.println(tern + " " + c);
        int n = 0;
        while ((n += 3) < 10) {
            c++;
        }
        System.out.println(n);
        List<Integer> out = new ArrayList<>();
        int q = 0;
        while (q++ < 3) {
            out.add(q);
        }
        System.out.println(out.size() + " " + q + " " + next() + next() + " " + calls);
        String s = "n";
        s += i++;
        System.out.println(s + " " + i);
    }
}
