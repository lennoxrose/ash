package ash.lang;

/// Types with a natural order.
public interface Comparable<T> {
    /// Negative, zero or positive as this value orders before, equal to or after the other.
    int compareTo(T other);
}
