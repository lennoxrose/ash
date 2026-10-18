package ash.lang;

/// Boxed char; represented as its code point.
public final class Character implements Comparable<Character> {
    /// The value as a char.
    @Lower("$0") public native char toCharValue();
    /// Negative, zero or positive as this value orders before, equal to or after the other.
    @Lower("__cmp($0, $1)") public native int compareTo(Character other);
    /// Converts the argument to this type (text for String).
    @Lower("$1") public static native Character valueOf(char v);
}
