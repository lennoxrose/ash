package ash.concurrent;

import ash.lang.function.Supplier;

/// Deterministic single-threaded executor: the Ash VM has no threads, so tasks run to
/// completion on the caller in submission order. Failures are captured in the Future.
public final class Executor {
    public Executor() { }

    /// Runs the task and captures its result or failure.
    public <T> Future<T> submit(Supplier<T> task) {
        try {
            return new Future<T>(task.get(), null);
        } catch (Exception e) {
            return new Future<T>(null, e);
        }
    }
}
