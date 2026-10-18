package ash.lang;

/// Thrown when an operation is not available.
public class UnsupportedOperationException extends RuntimeException {
    public UnsupportedOperationException() { }

    /// Creates a new UnsupportedOperationException.
    public UnsupportedOperationException(String message) {
        super(message);
    }

    /// Creates a new UnsupportedOperationException.
    public UnsupportedOperationException(String message, Throwable cause) {
        super(message, cause);
    }
}
