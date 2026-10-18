package ash.lang;

import ash.util.List;

/// Immutable text. Represented natively by the VM.
public final class String implements Comparable<String> {
    /// Number of characters.
    @Lower("len($0)") public native int length();
    /// Character code at the given index.
    @Lower("ord($0[$1])") public native char charAt(int index);
    /// Part of the text from the start index (to the end index, exclusive).
    @Lower("substring($0, $1, len($0))") public native String substring(int begin);
    /// Part of the text from the start index (to the end index, exclusive).
    @Lower("substring($0, $1, $2)") public native String substring(int begin, int end);
    /// Index of the first occurrence, or -1.
    @Lower("indexOf($0, $1)") public native int indexOf(String s);
    /// True when the value is present.
    @Lower("contains($0, $1)") public native bool contains(String s);
    /// True when the text begins with the prefix (at the offset, if given).
    @Lower("starts_with($0, $1)") public native bool startsWith(String s);
    /// True when the text begins with the prefix (at the offset, if given).
    @Lower("__startsat($0, $1, $2)") public native bool startsWith(String s, int offset);
    /// True when the text ends with the suffix.
    @Lower("ends_with($0, $1)") public native bool endsWith(String s);
    /// Replaces every occurrence of a substring.
    @Lower("replace($0, $1, $2)") public native String replace(String from, String to);
    /// The text repeated n times.
    @Lower("repeat($0, $1)") public native String repeat(int n);
    /// Upper-case copy.
    @Lower("upper($0)") public native String toUpperCase();
    /// Lower-case copy.
    @Lower("lower($0)") public native String toLowerCase();
    /// Copy without leading and trailing whitespace.
    @Lower("trim($0)") public native String trim();
    /// True when there are no elements or characters.
    @Lower("(len($0) == 0)") public native bool isEmpty();
    /// Splits at a separator; trailing empty parts are dropped.
    @Lower("__split($0, $1)") public native String[] split(String sep);
    /// Negative, zero or positive as this value orders before, equal to or after the other.
    @Lower("__cmp($0, $1)") public native int compareTo(String other);
    /// Converts the argument to this type (text for String).
    @Lower("__str($1)") public static native String valueOf(Object? o);
    /// Converts the argument to this type (text for String).
    @Lower("str($1)") public static native String valueOf(int i);
    /// Converts the argument to this type (text for String).
    @Lower("__lstr($1)") public static native String valueOf(long l);
    /// Converts the argument to this type (text for String).
    @Lower("__dstr($1)") public static native String valueOf(double d);
    /// Converts the argument to this type (text for String).
    @Lower("__fstr($1)") public static native String valueOf(float f);
    /// Converts the argument to this type (text for String).
    @Lower("__bs($1)") public static native String valueOf(bool b);
    /// Converts the argument to this type (text for String).
    @Lower("chr($1)") public static native String valueOf(char c);
    /// Joins strings with a separator.
    @Lower("join($2, $1)") public static native String join(String sep, List<String> parts);
}
