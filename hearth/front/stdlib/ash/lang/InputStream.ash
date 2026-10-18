package ash.lang;

/// Standard input; see System.in.
public final class InputStream {
    /// Reads one line from standard input.
    @Lower("input()") public native String readLine();
}
