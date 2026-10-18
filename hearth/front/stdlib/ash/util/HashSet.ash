package ash.util;

/// Set backed by an Ash map; any element type is supported.
public class HashSet<T> implements Set<T> {
    @Lower("{}") public HashSet() { }
    /// Number of elements.
    @Lower("__msize($0)") public native int size();
    /// True when there are no elements or characters.
    @Lower("(__msize($0) == 0)") public native bool isEmpty();
    /// Adds a value (at the index, if given).
    @Lower("__sadd($0, $1)") public native bool add(T value);
    /// True when the value is present.
    @Lower("__mhas($0, $1)") public native bool contains(T value);
    /// Removes and returns the entry.
    @Lower("__sremove($0, $1)") public native bool remove(T value);
    /// The elements as a List.
    @Lower("__mkeys($0)") public native List<T> toList();
    /// Cursor over the elements.
    @Lower("__iter(__mkeys($0))") public native Iterator<T> iterator();
}
