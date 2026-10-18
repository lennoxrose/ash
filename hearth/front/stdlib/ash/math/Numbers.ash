package ash.math;

/// Integer and floating helpers that the core Math class does not provide.
public final class Numbers {
    /// The ratio of a circle's circumference to its diameter.
    public static final double PI = 3.141592653589793;

    /// Integer power.
    public static long pow(long base, int exponent) {
        long result = 1;
        for (int i = 0; i < exponent; i++) {
            result *= base;
        }
        return result;
    }

    /// Greatest common divisor.
    public static long gcd(long a, long b) {
        long x = a < 0 ? -a : a;
        long y = b < 0 ? -b : b;
        while (y != 0) {
            long t = x % y;
            x = y;
            y = t;
        }
        return x;
    }

    /// Limits the value to the range.
    public static int clamp(int value, int low, int high) {
        return Math.max(low, Math.min(value, high));
    }

    /// Nearest whole number.
    public static long round(double value) {
        return (long) Math.floor(value + 0.5);
    }
}
