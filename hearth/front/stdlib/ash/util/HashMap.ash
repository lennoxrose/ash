package ash.util;

/// Map backed by an Ash map; any key type is supported.
public class HashMap<K, V> implements Map<K, V> {
    @Lower("{}") public HashMap() { }
    /// Number of elements.
    @Lower("__msize($0)") public native int size();
    /// Stores a value under a key; returns the previous value or null.
    @Lower("__mput($0, $1, $2)") public native V? put(K key, V value);
    /// Element at the index (or the stored value).
    @Lower("__mget($0, $1)") public native V? get(K key);
    /// True when the key is present.
    @Lower("__mhas($0, $1)") public native bool containsKey(K key);
    /// Removes and returns the entry.
    @Lower("__mremove($0, $1)") public native V? remove(K key);
    /// Keys in insertion order.
    @Lower("__mkeys($0)") public native List<K> keys();
    /// Values in insertion order.
    @Lower("__mvals($0)") public native List<V> values();
}
