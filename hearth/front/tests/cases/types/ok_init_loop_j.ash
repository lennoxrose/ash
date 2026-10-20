package t;

public class OkInitLoop {
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

    static int afterJump(int n) {
        int c;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            if (i % 2 == 0) { c = i; continue; }
            sum += c;
            c = i;
        }
        return sum;
    }
}
