// A finally block runs between a `break` / `continue` and the jump landing, so what it moves is moved after the loop.
// expect: use of moved value 's'
// expect: use of moved value 'u'
package t;

import ash.util.ArrayList;
import ash.util.List;

public class ErrFinallyJump {
    static List<String> keep = new ArrayList<>();
    static void consume(String s) { keep.add(s); }

    static int viaBreak(int n) {
        String s = "x" + n;
        while (true) {
            try {
                break;
            } finally {
                consume(s);
            }
        }
        return s.length();
    }

    static int viaContinue(int n) {
        String u = "y" + n;
        int i = 0;
        while (i < 1) {
            i++;
            try {
                continue;
            } finally {
                consume(u);
            }
        }
        return u.length();
    }
}
