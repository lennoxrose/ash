package ash.lang.function;

/// Operation on one value with no result.
public interface Consumer<T> {
    /// Performs the operation.
    void accept(T t);
}
