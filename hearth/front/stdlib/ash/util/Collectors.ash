package ash.util;

/// Ready-made Collector implementations for Stream.collect.
public final class Collectors {
    /// Collects into a new List.
    public static <T> Collector<T, List<T>> toList() {
        return items -> items;
    }

    /// Joins strings with a separator.
    public static Collector<String, String> joining(String separator) {
        return items -> String.join(separator, items);
    }

    /// Counts the elements.
    public static <T> Collector<T, Integer> counting() {
        return items -> items.size();
    }
}
