package ash.util;

import ash.lang.function.Function;

/// Success value or error value, for APIs that report failure without exceptions.
public final class Result<T, E> {
    private final T? value;
    private final E? error;

    private Result(T? value, E? error) {
        this.value = value;
        this.error = error;
    }

    /// Creates a successful result.
    public static <T, E> Result<T, E> ok(T value) {
        return new Result<T, E>(value, null);
    }

    /// Creates a failed result.
    public static <T, E> Result<T, E> err(E error) {
        return new Result<T, E>(null, error);
    }

    /// True for a successful result.
    public bool isOk() {
        return error == null;
    }

    /// Returns the value or throws IllegalStateException when this is an error.
    public T unwrap() {
        T? v = value;
        if (v == null) {
            throw new IllegalStateException("unwrap on an error result");
        }
        return v;
    }

    /// The value, or the fallback.
    public T unwrapOr(T fallback) {
        T? v = value;
        if (v == null) {
            return fallback;
        }
        return v;
    }

    /// The error of a failed result, or null.
    public E? getError() {
        return error;
    }

    /// Transforms every element.
    public <R> Result<R, E> map(Function<T, R> f) {
        T? v = value;
        E? e = error;
        if (v == null) {
            return new Result<R, E>(null, e);
        }
        return new Result<R, E>(f.apply(v), null);
    }
}
