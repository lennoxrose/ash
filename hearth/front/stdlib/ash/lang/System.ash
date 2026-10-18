package ash.lang;

/// Process-level facilities: standard streams and exit.
public final class System {
    /// Standard output.
    public static final PrintStream out = new PrintStream();
    /// Standard error (the VM has a single output stream, so it shares stdout).
    public static final PrintStream err = new PrintStream();
    /// Standard input.
    public static final InputStream in = new InputStream();
    /// Ends the program with the given status.
    @Lower("exit($1)") public static native void exit(int status);
}
