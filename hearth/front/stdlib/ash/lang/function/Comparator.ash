package ash.lang.function;

/// Defines an order between two values; negative, zero or positive.
public interface Comparator<T> {
    /// Orders two values: negative, zero or positive.
    int compare(T a, T b);
}
