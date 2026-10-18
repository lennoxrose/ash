package ash.util;

import ash.lang.function.Comparator;
import ash.lang.function.Consumer;

/// Growable list backed by an Ash array.
public class ArrayList<T> implements List<T> {
    @Lower("[]") public ArrayList() { }
    /// Number of elements.
    @Lower("len($0)") public native int size();
    /// True when there are no elements or characters.
    @Lower("(len($0) == 0)") public native bool isEmpty();
    /// Element at the index (or the stored value).
    @Lower("$0[$1]") public native T get(int index);
    /// Replaces the element at the index; returns the old one.
    @Lower("__lset($0, $1, $2)") public native T set(int index, T value);
    /// Adds a value (at the index, if given).
    @Lower("push($0, $1)") public native void add(T value);
    /// Adds a value (at the index, if given).
    @Lower("insert($0, $1, $2)") public native void add(int index, T value);
    /// Removes and returns the entry.
    @Lower("__lremove($0, $1)") public native T remove(int index);
    /// Removes and returns the last element.
    @Lower("pop($0)") public native T removeLast();
    /// True when the value is present.
    @Lower("__lcontains($0, $1)") public native bool contains(Object o);
    /// Sorts in place with the comparator.
    @Lower("sort($0, __cmpfn($1))") public native void sort(Comparator<T> c);
    /// Elements from the start index to the end index, exclusive.
    @Lower("slice($0, $1, $2)") public native List<T> subList(int from, int to);
    /// Pipeline over a copy of the elements.
    @Lower("slice($0, 0, len($0))") public native Stream<T> stream();
    /// Cursor over the elements.
    @Lower("__iter($0)") public native Iterator<T> iterator();
    /// Runs the action for every element.
    @Lower("__foreach($0, __fn1($1, \"accept/1\"))") public native void forEach(Consumer<T> c);
}
