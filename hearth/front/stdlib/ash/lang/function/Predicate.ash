package ash.lang.function;

/// Test of one value.
public interface Predicate<T> {
    /// Evaluates the predicate.
    bool test(T t);
}
