package ash.io;

/// A file system path, kept as text.
public record Path(String value) {
    /// Creates a value from the argument.
    public static Path of(String value) {
        return new Path(value);
    }

    /// Child path below this one.
    public Path resolve(String child) {
        if (value.endsWith("/")) {
            return new Path(value + child);
        }
        return new Path(value + "/" + child);
    }

    /// Text form.
    public String toString() {
        return value;
    }
}
