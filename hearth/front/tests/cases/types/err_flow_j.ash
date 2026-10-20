package t;

// expect: unreachable statement
// expect: missing return statement in 'viaField'
// expect: missing return statement in 'dynamic'
// expect: missing return statement in 'otherObject'
public class ErrFlow {
    final bool on = true;
    static final int LIMIT = 3;

    static int constantCondition() {
        while (1 < 2) { }
        return 1;                                         // unreachable: the condition is a constant expression
    }

    static int negated() {
        for (; !false && true; ) { }
        return 1;                                         // unreachable
    }

    static int finalLocal() {
        final bool t = true;
        do { } while (t);
        return 1;                                         // unreachable
    }

    int viaField(ErrFlow o) {
        while (o.on) { }                                  // o.on is not a constant expression, so the loop may end
    }

    int dynamic(int x) {
        while (x > 2) { }
    }

    int otherObject(ErrFlow o) {
        while (o.LIMIT > 2) { }                           // neither is o.LIMIT
    }
}
