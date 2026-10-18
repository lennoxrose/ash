package t;

// expect: effectively final
// expect: lambda has 2 parameter(s)
import ash.lang.function.Function;

class ErrLambda {
    static void f() {
        int n = 0;
        Function<Integer, Integer> a = x -> x + n;
        n = 5;
        Function<Integer, Integer> b = (x, y) -> x;
    }
}
