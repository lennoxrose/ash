package ash.time;

/// A length of time in milliseconds. (The VM has no clock; this is pure arithmetic.)
public record Duration(long millis) {
    /// Duration of whole seconds.
    public static Duration ofSeconds(long seconds) {
        return new Duration(seconds * 1000);
    }

    /// Duration of milliseconds.
    public static Duration ofMillis(long millis) {
        return new Duration(millis);
    }

    /// Sum of two values.
    public Duration plus(Duration other) {
        return new Duration(millis + other.millis());
    }

    /// Whole seconds.
    public long toSeconds() {
        return millis / 1000;
    }
}
