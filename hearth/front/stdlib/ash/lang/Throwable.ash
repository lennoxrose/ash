package ash.lang;

/// Root of everything that can be thrown; carries a message, a cause and the creation site.
public class Throwable {
    private final String? message;
    private final Throwable? cause;

    /// Creates a new Throwable.
    public Throwable() {
        this.message = null;
        this.cause = null;
    }

    /// Creates a new Throwable.
    public Throwable(String? message) {
        this.message = message;
        this.cause = null;
    }

    /// Creates a new Throwable.
    public Throwable(String? message, Throwable? cause) {
        this.message = message;
        this.cause = cause;
    }

    /// Detail message, or null.
    public String? getMessage() {
        return message;
    }

    /// Underlying cause, or null.
    public Throwable? getCause() {
        return cause;
    }

    /// Prints the exception, its creation site and its causes.
    @Lower("__pst($0)") public native void printStackTrace();
}
