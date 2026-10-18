package ash.test;

/// Minimal assertions; a failure throws AssertionError.
public final class Assert {
    /// Fails unless the condition holds.
    public static void assertTrue(bool condition, String message) {
        if (!condition) {
            throw new AssertionError(message);
        }
    }

    /// Fails unless the values are equal.
    public static void assertEquals(Object? expected, Object? actual) {
        if (expected == null) {
            if (actual != null) {
                throw new AssertionError("expected null but was " + actual);
            }
            return;
        }
        if (!expected.equals(actual)) {
            throw new AssertionError("expected " + expected + " but was " + actual);
        }
    }

    /// Fails unless the value is null.
    public static void assertNull(Object? actual) {
        if (actual != null) {
            throw new AssertionError("expected null but was " + actual);
        }
    }

    /// Fails unconditionally.
    public static void fail(String message) {
        throw new AssertionError(message);
    }
}
