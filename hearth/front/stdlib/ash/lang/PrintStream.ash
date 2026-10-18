package ash.lang;

/// Text output; System.out and System.err.
public final class PrintStream {
    /// Prints the value followed by a newline.
    @Lower("__println(\"\")") public native void println();
    /// Prints the value followed by a newline.
    @Lower("__println(__str($1))") public native void println(Object? o);
    /// Prints the value followed by a newline.
    @Lower("__println(__str($1))") public native void println(String? s);
    /// Prints the value followed by a newline.
    @Lower("__println(str($1))") public native void println(int i);
    /// Prints the value followed by a newline.
    @Lower("__println(__lstr($1))") public native void println(long l);
    /// Prints the value followed by a newline.
    @Lower("__println(__dstr($1))") public native void println(double d);
    /// Prints the value followed by a newline.
    @Lower("__println(__fstr($1))") public native void println(float f);
    /// Prints the value followed by a newline.
    @Lower("__println(__bs($1))") public native void println(bool b);
    /// Prints the value followed by a newline.
    @Lower("__println(chr($1))") public native void println(char c);
}
