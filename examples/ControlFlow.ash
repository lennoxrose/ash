package examples;

public class ControlFlow {
    public const int LIMIT = 20;

    static String classify(int n) {
        if (n < 0) {
            return "negative";
        } else if (n == 0) {
            return "zero";
        } else if (n < 10) {
            return "small";
        } else {
            return "large";
        }
    }

    public static void main(String[] args) {
        System.out.println(classify(-5) + " " + classify(0) + " " + classify(7) + " " + classify(LIMIT));
        int i = 0;
        int evens = 0;
        while (i < 10) {
            i++;
            if (i % 2 == 1) {
                continue;
            }
            evens += i;
            if (evens > 20) {
                break;
            }
        }
        System.out.println(i + " " + evens);
        int countdown = 3;
        do {
            System.out.println("t-" + countdown);
            countdown--;
        } while (countdown > 0);
        var flags = 5;
        flags <<= 2;
        flags |= 3;
        System.out.println(flags + " " + (flags & 6) + " " + (flags ^ 1) + " " + (flags >> 1));
        String word = "control";
        System.out.println(word.substring(0, 3) + word.toUpperCase().substring(3) + " " + word.indexOf("tr"));
        for (int a = 0, step = 0; a < 5; a += 2, step++) {
            System.out.println("a=" + a + " step=" + step);
        }
    }
}
