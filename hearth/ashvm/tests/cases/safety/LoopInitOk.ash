public class LoopInitOk {
    // the read is behind a condition, a jump or an inner loop: an earlier iteration may have assigned it
    static int guarded(int n) {
        int c;
        bool first = true;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            if (!first) sum += c;
            first = false;
            c = i;
        }
        return sum;
    }

    static int afterContinue(int n) {
        int c;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            if (i % 2 == 0) { c = i; continue; }
            sum += c;
            c = i;
        }
        return sum;
    }

    static int shortCircuit(int n) {
        int c;
        int sum = 0;
        bool seen = false;
        for (int i = 0; i < n; i++) {
            if (seen && c > 0) sum++;
            seen = true;
            c = i;
        }
        return sum;
    }

    static int ternary(int n) {
        int c;
        bool seen = false;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            sum += seen ? c : 0;
            seen = true;
            c = i;
        }
        return sum;
    }

    static int assignedFirst(int n) {
        int c;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            c = i;
            sum += c;
        }
        return sum;
    }

    static int inner(int n) {
        int c;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < 2; j++) {
                if (i > 0) sum += c;
            }
            c = i;
        }
        return sum;
    }

    static int assignedBeforeLoop(int n) {
        int c;
        if (n > 0) c = 1;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            sum += c;
            c = i;
        }
        return sum;
    }

    static int innerLoopLater(int n) {
        int c;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < 1; j++) {
                sum += i > 0 ? c : 0;
            }
            c = i;
        }
        return sum;
    }

    static int guardedRange(int n) {
        int c;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            if (i > 0) sum += c;
            c = i;
        }
        return sum;
    }

    static int tryCatch(int n) {
        int c;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            try {
                sum += c;
            } catch (Exception e) {
                sum += 100;
            }
            c = i;
        }
        return sum;
    }

    public static void main(String[] args) {
        System.out.println(guarded(4));
        System.out.println(afterContinue(5));
        System.out.println(shortCircuit(4));
        System.out.println(ternary(4));
        System.out.println(assignedFirst(4));
        System.out.println(inner(4));
        System.out.println(assignedBeforeLoop(3));
        System.out.println(innerLoopLater(3));
        System.out.println(guardedRange(4));
    }
}
