package ash.util;

import ash.lang.Iterable;

/// Collection without duplicates.
public interface Set<T> extends Iterable<T> {
    /// Number of elements.
    @Lower("__msize($0)") int size();
    /// True when there are no elements or characters.
    @Lower("(__msize($0) == 0)") bool isEmpty();
    /// Adds a value (at the index, if given).
    @Lower("__sadd($0, $1)") bool add(T value);
    /// True when the value is present.
    @Lower("__mhas($0, $1)") bool contains(T value);
    /// Removes and returns the entry.
    @Lower("__sremove($0, $1)") bool remove(T value);
    /// The elements as a List.
    @Lower("__mkeys($0)") List<T> toList();
}
