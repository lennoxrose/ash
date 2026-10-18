package ash.lang;

/// Base class of every enum: constant name and declaration order.
public abstract class Enum<E> implements Comparable<E> {
    /// Declared name of the constant.
    @Lower("$0[\"__name\"]") public final native String name();
    /// Position in the enum declaration, starting at 0.
    @Lower("$0[\"__ord\"]") public final native int ordinal();
    /// Negative, zero or positive as this value orders before, equal to or after the other.
    @Lower("($0[\"__ord\"] - $1[\"__ord\"])") public final native int compareTo(E other);

    /// Text form.
    public String toString() {
        return name();
    }
}
