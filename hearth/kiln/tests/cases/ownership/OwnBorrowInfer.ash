public class OwnBorrowInfer {
    static class Box {
        String held;
        void put(String s) { held = s; }                 // keeps its argument: passing moves
    }

    static int size(String s) { return s.length(); }     // only reads: the caller keeps ownership
    static int sum(int[] xs) {
        int t = 0;
        for (int x : xs) t += x;
        return t;
    }
    static int twice(String s) { return size(s) + size(s); }   // passes it on to a reader: still a reader
    static String same(String s) { return s; }           // returns the caller's value: moves

    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 20000; i++) {
            String a = "item" + i;
            int[] xs = {i, 1, 2};
            total += size(a) + size(a) + twice(a) + sum(xs) + sum(xs);   // no move, and a / xs are still freed each iteration
        }
        String keep = "kept";
        Box b = new Box();
        b.put(keep);
        System.out.println(total + " " + b.held);
        String r = "r" + total;
        String back = same(r);
        System.out.println(back);
    }
}
