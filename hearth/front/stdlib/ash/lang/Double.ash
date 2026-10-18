package ash.lang;

/// Boxed double; represented as a plain number.
public final class Double extends Number implements Comparable<Double> {
    /// The value as a double.
    @Lower("$0") public native double toDoubleValue();
    /// Negative, zero or positive as this value orders before, equal to or after the other.
    @Lower("__cmp($0, $1)") public native int compareTo(Double other);
    /// Converts the argument to this type (text for String).
    @Lower("$1") public static native Double valueOf(double v);
    /// Parses text; throws ParseException when malformed.
    @Lower("__parse_double($1)") public static native double parse(String s) throws ParseException;
}
