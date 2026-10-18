package ash.lang;

/// Boxed byte; represented as a plain number.
public final class Byte extends Number implements Comparable<Byte> {
    /// The value as a double.
    @Lower("$0") public native double toDoubleValue();
    /// The value as a byte.
    @Lower("$0") public native byte toByteValue();
    /// Negative, zero or positive as this value orders before, equal to or after the other.
    @Lower("__cmp($0, $1)") public native int compareTo(Byte other);
    /// Converts the argument to this type (text for String).
    @Lower("$1") public static native Byte valueOf(byte v);
    /// Parses text; throws ParseException when malformed.
    @Lower("__parse_int($1)") public static native byte parse(String s) throws ParseException;
}
