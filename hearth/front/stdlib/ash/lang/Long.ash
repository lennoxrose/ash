package ash.lang;

/// Boxed long; a plain number while it fits in 2^53, an exact two-halves value beyond that (--wrap-int).
public final class Long extends Number implements Comparable<Long> {
    /// Largest long value, 2^63 - 1 (exact with --wrap-int; the nearest double otherwise).
    @Lower("9223372036854775807||{\"__L\": 1, \"hi\": 2147483647, \"lo\": 4294967295}") public static final long MAX_VALUE = 9223372036854775807L;
    /// Smallest long value, -2^63 (exact with --wrap-int; the nearest double otherwise).
    @Lower("(-9223372036854775807)||{\"__L\": 1, \"hi\": -2147483648, \"lo\": 0}") public static final long MIN_VALUE = -9223372036854775807L - 1L;
    /// The value as a double.
    @Lower("__l2d($0)") public native double toDoubleValue();
    /// The value as a long.
    @Lower("$0") public native long toLongValue();
    /// Negative, zero or positive as this value orders before, equal to or after the other.
    @Lower("__lcmp($0, $1)") public native int compareTo(Long other);
    /// Converts the argument to this type (text for String).
    @Lower("$1") public static native Long valueOf(long v);
    /// Parses text; throws ParseException when malformed.
    @Lower("__lparse($1)") public static native long parse(String s) throws ParseException;
}
