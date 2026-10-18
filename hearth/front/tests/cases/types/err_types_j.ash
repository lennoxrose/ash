package t;

// expect: incompatible types in initializer: String cannot be converted to int
// expect: possible lossy conversion
// expect: condition must be of type bool
public class ErrTypes {
    static void f() {
        int a = "x";
        int b = 5;
        byte c = b;
        if (b) { }
    }
}
