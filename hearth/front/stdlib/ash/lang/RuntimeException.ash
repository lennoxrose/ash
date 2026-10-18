package ash.lang;

/// Base of unchecked failures that signal programming errors.
public class RuntimeException extends Exception {
    public RuntimeException() { }

    /// Creates a new RuntimeException.
    public RuntimeException(String message) {
        super(message);
    }

    /// Creates a new RuntimeException.
    public RuntimeException(String message, Throwable cause) {
        super(message, cause);
    }
}
