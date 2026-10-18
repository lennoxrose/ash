package ash.lang;

/// Boxed bool; the VM represents it as the plain value.
public final class Boolean implements Comparable<Boolean> {
    /// The value as a bool.
    @Lower("$0") public native bool toBoolValue();
    /// Negative, zero or positive as this value orders before, equal to or after the other.
    @Lower("__cmp($0, $1)") public native int compareTo(Boolean other);
    /// Converts the argument to this type (text for String).
    @Lower("$1") public static native Boolean valueOf(bool v);
    /// Parses text; throws ParseException when malformed.
    @Lower("($1 == \"true\")") public static native bool parse(String s);
}
