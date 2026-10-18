package ash.util;

import ash.lang.function.*;

/// Eager collection pipeline over a List.
public interface Stream<T> {
    /// Keeps the elements that pass the test.
    @Lower("filter($0, __fn1($1, \"test/1\"))") Stream<T> filter(Predicate<T> p);
    /// Transforms every element.
    @Lower("map($0, __fn1($1, \"apply/1\"))") <R> Stream<R> map(Function<T, R> f);
    /// Folds the elements into one value starting from the identity.
    @Lower("reduce($0, __fn2($2, \"apply/2\"), $1)") T reduce(T identity, BiFunction<T, T, T> f);
    /// The elements as a List.
    @Lower("slice($0, 0, len($0))") List<T> toList();
    /// Runs the action for every element.
    @Lower("__foreach($0, __fn1($1, \"accept/1\"))") void forEach(Consumer<T> c);
    /// Applies a Collector to the elements.
    @Lower("__sam1($1, \"finish/1\", $0)") <R> R collect(Collector<T, R> collector);
    /// Number of elements.
    @Lower("len($0)") int count();
    /// Sorted copy using the comparator.
    @Lower("__sorted($0, __cmpfn($1))") Stream<T> sorted(Comparator<T> c);
    /// True when some element passes the test.
    @Lower("__anymatch($0, __fn1($1, \"test/1\"))") bool anyMatch(Predicate<T> p);
}
