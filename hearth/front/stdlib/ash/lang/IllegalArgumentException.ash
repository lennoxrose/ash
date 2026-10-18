package ash.lang;

/// Thrown when an argument is not acceptable.
public class IllegalArgumentException extends RuntimeException {
    public IllegalArgumentException() { }

    /// Creates a new IllegalArgumentException.
    public IllegalArgumentException(String message) {
        super(message);
    }

    /// Creates a new IllegalArgumentException.
    public IllegalArgumentException(String message, Throwable cause) {
        super(message, cause);
    }
}
