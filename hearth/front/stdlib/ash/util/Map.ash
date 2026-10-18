package ash.util;

/// Association of keys to values in insertion order.
public interface Map<K, V> {
    /// Number of elements.
    @Lower("__msize($0)") int size();
    /// Stores a value under a key; returns the previous value or null.
    @Lower("__mput($0, $1, $2)") V? put(K key, V value);
    /// Element at the index (or the stored value).
    @Lower("__mget($0, $1)") V? get(K key);
    /// True when the key is present.
    @Lower("__mhas($0, $1)") bool containsKey(K key);
    /// Removes and returns the entry.
    @Lower("__mremove($0, $1)") V? remove(K key);
    /// Keys in insertion order.
    @Lower("__mkeys($0)") List<K> keys();
    /// Values in insertion order.
    @Lower("__mvals($0)") List<V> values();
}
