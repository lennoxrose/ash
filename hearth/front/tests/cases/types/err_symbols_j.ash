package t;

// expect: cannot find symbol 'nope'
// expect: cannot find method 'missing'
// expect: cannot find type 'Ghost'
public class ErrSymbols {
    static void f() {
        int a = nope;
        missing(1);
        Ghost g = null;
    }
}
