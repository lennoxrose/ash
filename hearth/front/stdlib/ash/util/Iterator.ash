package ash.util;

/// Cursor over a sequence of values.
public interface Iterator<T> {
    /// True when next() will return another value.
    bool hasNext();
    /// Returns the next value.
    T next();
}
