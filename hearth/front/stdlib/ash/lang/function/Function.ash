package ash.lang.function;

/// Function of one argument.
public interface Function<T, R> {
    /// Applies the function.
    R apply(T t);
}
