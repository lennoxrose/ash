package ash.lang.function;

/// Source of values with no arguments.
public interface Supplier<T> {
    /// Element at the index (or the stored value).
    T get();
}
