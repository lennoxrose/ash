package ash.lang;

/// Thrown for integer division or remainder by zero.
public class ArithmeticException extends RuntimeException {
    public ArithmeticException() { }

    /// Creates a new ArithmeticException.
    public ArithmeticException(String message) {
        super(message);
    }

    /// Creates a new ArithmeticException.
    public ArithmeticException(String message, Throwable cause) {
        super(message, cause);
    }
}
