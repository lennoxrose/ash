package t;

// expect: possible null dereference
public class ErrNullDeref {
    static int f(String? s) {
        return s.length();
    }
}
