public class OwnMovedKeep {
    static String same(String s) { return s; }
    static class Box {
        String held;
        void put(String s) { held = s; }
    }

    public static void main(String[] args) {
        String a = "abc";
        String back = same(a);            // returns its parameter: a is moved
        System.out.println(a + back);     // error
        Box b = new Box();
        String c = "def";
        b.put(c);                         // stores its parameter: c is moved
        System.out.println(c);            // error
    }
}
