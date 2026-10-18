package ash.lang;

import ash.util.Iterator;

/// Types that can be the target of a for-each loop.
public interface Iterable<T> {
    /// Cursor over the elements.
    @Lower("__iterof($0)") Iterator<T> iterator();
}
