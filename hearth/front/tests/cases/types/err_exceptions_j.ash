package t;

// expect: unreported exception IOException
// expect: is never thrown in the body
public class ErrExceptions {
    static void risky() throws IOException { }

    static void f() {
        risky();
        try {
            int x = 1;
        } catch (ParseException e) { }
    }
}
