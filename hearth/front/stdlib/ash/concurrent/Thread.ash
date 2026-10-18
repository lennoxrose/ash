package ash.concurrent;

import ash.lang.function.Runnable;

/// A unit of work. The Ash VM has no threads, so `start()` runs the task to completion on the
/// caller and `join()` returns immediately; programs written against it stay deterministic.
public class Thread {
    private final Runnable task;
    private bool finished = false;

    /// Creates a new Thread.
    public Thread(Runnable task) {
        this.task = task;
    }

    /// Runs the task now, on the calling flow of control.
    public void start() {
        if (!finished) {
            finished = true;
            task.run();
        }
    }

    /// Returns immediately: the task has already completed in start().
    public void join() { }

    /// True once the task has run.
    public bool isFinished() {
        return finished;
    }
}
