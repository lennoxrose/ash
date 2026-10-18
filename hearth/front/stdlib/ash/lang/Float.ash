package ash.lang;

/// Boxed float; represented as a plain number.
public final class Float extends Number implements Comparable<Float> {
    /// The value as a double.
    @Lower("$0") public native double toDoubleValue();
    /// The value as a float.
    @Lower("$0") public native float toFloatValue();
    /// Negative, zero or positive as this value orders before, equal to or after the other.
    @Lower("__cmp($0, $1)") public native int compareTo(Float other);
    /// Converts the argument to this type (text for String).
    @Lower("$1") public static native Float valueOf(float v);
    /// Parses text; throws ParseException when malformed.
    @Lower("__parse_double($1)") public static native float parse(String s) throws ParseException;
}
