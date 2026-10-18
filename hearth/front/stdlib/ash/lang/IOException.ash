package ash.lang;

/// Checked failure of a file or stream operation.
public class IOException extends Exception {
    public IOException() { }

    /// Creates a new IOException.
    public IOException(String message) {
        super(message);
    }

    /// Creates a new IOException.
    public IOException(String message, Throwable cause) {
        super(message, cause);
    }
}
