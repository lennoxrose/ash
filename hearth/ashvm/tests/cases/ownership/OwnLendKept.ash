public class OwnLendKept {
    static class Box {
        String held;
        void put(String s) { held = s; }     // keeps its parameter
    }
    static String same(String s) { return s; }   // returns its parameter
    static int size(String s) { return s.length(); }

    public static void main(String[] args) {
        Box b = new Box();
        String a = "abc";
        b.put(&a);                           // error: put keeps a, so it cannot be a loan (Box has no subclass, so the target is known)
        String c = "def";
        String d = same(&c);                 // error: same returns its parameter
        String e = "ghi";
        int n = size(&e);                    // fine: size only reads
        System.out.println(n + b.held + d);
    }
}
