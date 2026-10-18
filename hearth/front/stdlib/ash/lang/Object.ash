package ash.lang;

/// Root of the class hierarchy.
public class Object {
    @Lower("{}") public Object() { }
    /// Value equality; identity unless overridden.
    @Lower("__eq($0, $1)") public native bool equals(Object? o);
    /// Hash consistent with equals.
    @Lower("__hash($0)") public native int hashCode();
    /// Text form.
    @Lower("__str($0)") public native String toString();
    /// Runtime class of the object.
    @Lower("__classof($0)") public native Class getClass();
}
