package ash.lang;

/// Boxed int; represented as a plain number.
public final class Integer extends Number implements Comparable<Integer> {
    /// Largest int value, 2^31 - 1.
    @Lower("2147483647") public static final int MAX_VALUE = 2147483647;
    /// Smallest int value, -2^31.
    @Lower("(-2147483648)") public static final int MIN_VALUE = -2147483648;
    /// The value as a double.
    @Lower("$0") public native double toDoubleValue();
    /// The value as an int.
    @Lower("$0") public native int toIntValue();
    /// Negative, zero or positive as this value orders before, equal to or after the other.
    @Lower("__cmp($0, $1)") public native int compareTo(Integer other);
    /// Converts the argument to this type (text for String).
    @Lower("$1") public static native Integer valueOf(int v);
    /// Parses text; throws ParseException when malformed.
    @Lower("__parse_int($1)") public static native int parse(String s) throws ParseException;
}
