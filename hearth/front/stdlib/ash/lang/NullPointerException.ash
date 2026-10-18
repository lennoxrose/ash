package ash.lang;

/// Thrown when a null reference is used.
public class NullPointerException extends RuntimeException {
    public NullPointerException() { }

    /// Creates a new NullPointerException.
    public NullPointerException(String message) {
        super(message);
    }

    /// Creates a new NullPointerException.
    public NullPointerException(String message, Throwable cause) {
        super(message, cause);
    }
}
