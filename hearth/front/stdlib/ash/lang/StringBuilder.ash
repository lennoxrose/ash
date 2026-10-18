package ash.lang;

/// Mutable text accumulator; avoids quadratic string building.
public final class StringBuilder {
    @Lower("{\"p\": []}") public StringBuilder() { }
    /// Adds text to the end; returns this builder.
    @Lower("__sbappend($0, __str($1))") public native StringBuilder append(String? s);
    /// Adds text to the end; returns this builder.
    @Lower("__sbappend($0, __str($1))") public native StringBuilder append(Object? o);
    /// Adds text to the end; returns this builder.
    @Lower("__sbappend($0, str($1))") public native StringBuilder append(int i);
    /// Adds text to the end; returns this builder.
    @Lower("__sbappend($0, chr($1))") public native StringBuilder append(char c);
    /// Number of characters.
    @Lower("len(__sbstr($0))") public native int length();
    /// Text form.
    @Lower("__sbstr($0)") public native String toString();
}
