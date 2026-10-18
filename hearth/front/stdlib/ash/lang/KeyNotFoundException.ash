package ash.lang;

/// Thrown when a map lookup that must succeed finds no entry.
public class KeyNotFoundException extends RuntimeException {
    public KeyNotFoundException() { }

    /// Creates a new KeyNotFoundException.
    public KeyNotFoundException(String message) {
        super(message);
    }

    /// Creates a new KeyNotFoundException.
    public KeyNotFoundException(String message, Throwable cause) {
        super(message, cause);
    }
}
