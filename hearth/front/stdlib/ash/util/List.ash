package ash.util;

import ash.lang.Iterable;
import ash.lang.function.Comparator;
import ash.lang.function.Consumer;

/// Ordered, indexable collection.
public interface List<T> extends Iterable<T> {
    /// Number of elements.
    @Lower("len($0)") int size();
    /// True when there are no elements or characters.
    @Lower("(len($0) == 0)") bool isEmpty();
    /// Element at the index (or the stored value).
    @Lower("$0[$1]") T get(int index);
    /// Replaces the element at the index; returns the old one.
    @Lower("__lset($0, $1, $2)") T set(int index, T value);
    /// Adds a value (at the index, if given).
    @Lower("push($0, $1)") void add(T value);
    /// Adds a value (at the index, if given).
    @Lower("insert($0, $1, $2)") void add(int index, T value);
    /// Removes and returns the entry.
    @Lower("__lremove($0, $1)") T remove(int index);
    /// Removes and returns the last element.
    @Lower("pop($0)") T removeLast();
    /// True when the value is present.
    @Lower("__lcontains($0, $1)") bool contains(Object o);
    /// Sorts in place with the comparator.
    @Lower("sort($0, __cmpfn($1))") void sort(Comparator<T> c);
    /// Elements from the start index to the end index, exclusive.
    @Lower("slice($0, $1, $2)") List<T> subList(int from, int to);
    /// Pipeline over a copy of the elements.
    @Lower("slice($0, 0, len($0))") Stream<T> stream();
    /// Runs the action for every element.
    @Lower("__foreach($0, __fn1($1, \"accept/1\"))") void forEach(Consumer<T> c);
}
