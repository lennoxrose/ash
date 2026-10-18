package ash.lang;

/// Thrown for an array or list index outside its bounds.
public class IndexOutOfBoundsException extends RuntimeException {
    public IndexOutOfBoundsException() { }

    /// Creates a new IndexOutOfBoundsException.
    public IndexOutOfBoundsException(String message) {
        super(message);
    }

    /// Creates a new IndexOutOfBoundsException.
    public IndexOutOfBoundsException(String message, Throwable cause) {
        super(message, cause);
    }
}
