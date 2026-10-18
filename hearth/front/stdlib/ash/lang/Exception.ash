package ash.lang;

/// Base of recoverable (checked) failures; RuntimeException subclasses are unchecked.
public class Exception extends Throwable {
    public Exception() { }

    /// Creates a new Exception.
    public Exception(String message) {
        super(message);
    }

    /// Creates a new Exception.
    public Exception(String message, Throwable cause) {
        super(message, cause);
    }
}
