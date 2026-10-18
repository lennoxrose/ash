package ash.lang.function;

/// Function of two arguments.
public interface BiFunction<A, B, R> {
    /// Applies the function.
    R apply(A a, B b);
}
