package ash.test;

/// Thrown by ash.test.Assert when a check fails.
public class AssertionError extends RuntimeException {
    /// Creates a new AssertionError.
    public AssertionError(String message) {
        super(message);
    }
}
