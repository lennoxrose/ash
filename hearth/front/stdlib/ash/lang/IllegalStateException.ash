package ash.lang;

/// Thrown when an object is not in a state that allows the call.
public class IllegalStateException extends RuntimeException {
    public IllegalStateException() { }

    /// Creates a new IllegalStateException.
    public IllegalStateException(String message) {
        super(message);
    }

    /// Creates a new IllegalStateException.
    public IllegalStateException(String message, Throwable cause) {
        super(message, cause);
    }
}
