/// A project @Lower native that only reads its String (a pure template, primitive result) borrows it, so a method
/// built on it borrows its own parameter too and the caller keeps the String.
class Raw {
    @Lower("ord(substring($1, $2, $2 + 1))") static native int charAt(String s, int i);
    @Lower("push($1, $2)") static native void put(int[] xs, int v);       // stores into its argument: not a read
}

class Utf8 {
    static int length(String s, int i) {
        int n = 0;
        while (i < s.length()) { n += Raw.charAt(s, i) > 0 ? 1 : 0; i++; }
        return n;
    }
}

public class OwnNativeRead {
    public static void main(String[] args) {
        String s = "héllo" + args.length;
        System.out.println(Utf8.length(s, 0) + Utf8.length(s, 1));   // s is used twice
        System.out.println(s);
    }
}
