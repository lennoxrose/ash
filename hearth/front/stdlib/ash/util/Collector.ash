package ash.util;

/// Turns the elements of a Stream into a single result; see Collectors.
public interface Collector<T, R> {
    /// Combines all elements into the result.
    R finish(List<T> items);
}
