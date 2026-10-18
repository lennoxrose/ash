package t;

// expect: cannot be used for initializer of non-null type String
// expect: possible null: argument
public class ErrNullAssign {
    static void take(String s) { }

    static void g(String? m) {
        String a = null;
        take(m);
    }
}
