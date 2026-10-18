package t;

// expect: does not cover all possible input values
// expect: missing return statement
// expect: 'break' outside
enum Color { RED, GREEN }

class ErrSwitch {
    static String f(Color c) {
        return switch (c) {
            case RED -> "r";
        };
    }

    static int g(int x) {
        if (x > 0) { return 1; }
    }

    static void h() {
        break;
    }
}
