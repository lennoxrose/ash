package ash.util;

import ash.lang.function.Function;

/// A value that may be absent. Prefer `T?` for fields and locals; use Optional for return values.
public final class Optional<T> {
    private final T? value;

    private Optional(T? value) {
        this.value = value;
    }

    /// Creates a value from the argument.
    public static <T> Optional<T> of(T value) {
        return new Optional<T>(value);
    }

    /// Empty value.
    public static <T> Optional<T> empty() {
        return new Optional<T>(null);
    }

    /// True when a value is present.
    public bool isPresent() {
        return value != null;
    }

    /// Element at the index (or the stored value).
    public T get() {
        T? v = value;
        if (v == null) {
            throw new IllegalStateException("Optional is empty");
        }
        return v;
    }

    /// The value, or the fallback.
    public T orElse(T other) {
        T? v = value;
        if (v == null) {
            return other;
        }
        return v;
    }

    /// Transforms every element.
    public <R> Optional<R> map(Function<T, R> f) {
        T? v = value;
        if (v == null) {
            return Optional.empty();
        }
        return Optional.of(f.apply(v));
    }
}
