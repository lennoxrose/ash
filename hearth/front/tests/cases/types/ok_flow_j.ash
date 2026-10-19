package t;

// A loop whose condition is a constant expression that is true (JLS 15.29: literals, ! && || == !=, comparisons of
// int constants, final variables initialised with constants) never ends, so no `return` is needed after it.
public class OkFlow {
    static final bool T = true;
    static final int LIMIT = 3;
    final bool on = true;

    static int notFalse() { while (!false) { } }
    static int compare() { while (1 < 2) { } }
    static int finalLocal() { final bool t = true; while (t) { } }
    static int staticFinal() { while (T) { } }
    static int both() { for (; true && true; ) { } }
    static int limit() { while (OkFlow.LIMIT > 2) { } }
    static int viaLocal() { final int q = 5; while (q > 2 && !(q == 4)) { } }
    int field() { while (on) { } }
    static int doWhile() { do { } while (1 == 1); }
    static int plain() { while (true) { } }
}
