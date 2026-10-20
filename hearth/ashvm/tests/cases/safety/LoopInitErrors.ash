public class LoopInitErrors {
    static int forLoop(int n) {
        int c;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            sum += c;
            c = i;
        }
        return sum;
    }

    static void whileLoop(int n) {
        String last;
        int i = 0;
        while (i < n) {
            System.out.println(last);
            last = "v" + i;
            i++;
        }
    }

    static int inCondition() {
        int c;
        int k = 0;
        while (c < 3) {
            c = k;
            k++;
        }
        return k;
    }

    static int doLoop() {
        int c;
        int k = 0;
        do {
            k += c + 1;
            c = k;
        } while (k < 5);
        return k;
    }

    static int afterSimpleStatements(int n) {
        int c;
        int t = 0;
        for (int i = 0; i < n; i++) {
            int d = i * 2;
            t += d;
            if (t > 100) System.out.println("big");
            t += c;
            c = t;
        }
        return t;
    }

    public static void main(String[] args) {
        System.out.println(forLoop(3));
    }
}
