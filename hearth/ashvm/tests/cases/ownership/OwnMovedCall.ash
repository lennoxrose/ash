public class OwnMovedCall {
    static String same(String s) { return s; }        // hands its parameter back: the caller's value is moved in

    public static void main(String[] args) {
        String a = "abc";
        String n = same(a);               // moves a into the callee
        System.out.println(a + n);        // error: a was moved by the call
    }
}
