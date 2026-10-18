package ash.lang;

/// Runtime type descriptor returned by Object.getClass().
public final class Class {
    /// Fully qualified class name.
    @Lower("$0[\"n\"]") public native String getName();
}
