package ash.lang;

/// Checked failure to parse text (numbers, JSON, URLs).
public class ParseException extends Exception {
    public ParseException() { }

    /// Creates a new ParseException.
    public ParseException(String message) {
        super(message);
    }

    /// Creates a new ParseException.
    public ParseException(String message, Throwable cause) {
        super(message, cause);
    }
}
