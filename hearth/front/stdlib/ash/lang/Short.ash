package ash.lang;

/// Boxed short; represented as a plain number.
public final class Short extends Number implements Comparable<Short> {
    /// The value as a double.
    @Lower("$0") public native double toDoubleValue();
    /// The value as a short.
    @Lower("$0") public native short toShortValue();
    /// Negative, zero or positive as this value orders before, equal to or after the other.
    @Lower("__cmp($0, $1)") public native int compareTo(Short other);
    /// Converts the argument to this type (text for String).
    @Lower("$1") public static native Short valueOf(short v);
    /// Parses text; throws ParseException when malformed.
    @Lower("__parse_int($1)") public static native short parse(String s) throws ParseException;
}
