import ash.util.ArrayList;
import ash.util.List;

public class OwnFreeLists {
    static class Bag {
        List<Integer> items = new ArrayList<>();      // owned: only filled and read through list methods
        List<Integer> view;                           // assigned from elsewhere: shared
        void put(int v) { items.add(v); }
        int sum() {
            int t = 0;
            for (int i = 0; i < items.size(); i++) t += items.get(i);
            return t;
        }
    }

    static List<Integer> build(int n) {
        List<Integer> out = new ArrayList<>();
        for (int i = 0; i < n; i++) out.add(i);
        return out;                                   // handed to the caller
    }

    static List<Integer> keep;

    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 20000; i++) {
            List<Integer> xs = new ArrayList<>();     // sole owner: freed each iteration
            xs.add(i);
            xs.add(i + 1);
            xs.set(0, 5);
            total += xs.size() + xs.get(1);
            List<Integer> ys = build(3);              // fresh from a factory
            for (int y : ys) total += y;
            List<Integer> zs = new ArrayList<>();     // passed on: not freed
            zs.add(7);
            if (i == 3) keep = zs;
            Bag b = new Bag();
            b.put(i);
            b.put(2);
            total += b.sum();
            List<Integer> ws = new ArrayList<>();
            ws.add(1);
            List<Integer> sub = ws.subList(0, 1);     // a copy / view: ws is not a sole owner of what sub sees
            total += sub.size();
        }
        System.out.println(total + " " + keep.size() + " " + keep.get(0));
    }
}
