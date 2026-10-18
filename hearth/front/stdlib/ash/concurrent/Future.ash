package ash.concurrent;

/// Result of a task run by an Executor.
public final class Future<T> {
    private final T? value;
    private final Exception? failure;

    /// Creates a new Future.
    Future(T? value, Exception? failure) {
        this.value = value;
        this.failure = failure;
    }

    /// True when the task threw.
    public bool isFailed() {
        return failure != null;
    }

    /// Returns the task result or rethrows its failure wrapped in IllegalStateException.
    public T get() {
        Exception? f = failure;
        if (f != null) {
            throw new IllegalStateException("task failed: " + f.getMessage(), f);
        }
        T? v = value;
        if (v == null) {
            throw new IllegalStateException("task produced no value");
        }
        return v;
    }
}
