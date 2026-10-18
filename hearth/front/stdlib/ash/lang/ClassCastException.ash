package ash.lang;

/// Thrown when a cast does not match the runtime type.
public class ClassCastException extends RuntimeException {
    public ClassCastException() { }

    /// Creates a new ClassCastException.
    public ClassCastException(String message) {
        super(message);
    }

    /// Creates a new ClassCastException.
    public ClassCastException(String message, Throwable cause) {
        super(message, cause);
    }
}
