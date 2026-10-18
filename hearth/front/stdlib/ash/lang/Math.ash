package ash.lang;

/// Basic numeric functions.
public final class Math {
    /// Square root.
    @Lower("sqrt($1)") public static native double sqrt(double x);
    /// Largest whole number not above the argument.
    @Lower("floor($1)") public static native double floor(double x);
    /// Absolute value.
    @Lower("abs($1)") public static native int abs(int x);
    /// Absolute value.
    @Lower("abs($1)") public static native double abs(double x);
    /// Larger of two values.
    @Lower("__max($1, $2)") public static native int max(int a, int b);
    /// Smaller of two values.
    @Lower("__min($1, $2)") public static native int min(int a, int b);
    /// Larger of two values.
    @Lower("__max($1, $2)") public static native double max(double a, double b);
    /// Smaller of two values.
    @Lower("__min($1, $2)") public static native double min(double a, double b);
}
