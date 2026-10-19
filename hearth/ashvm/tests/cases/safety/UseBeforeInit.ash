public class UseBeforeInit {
    static int plain() {
        int total;
        return total + 1;
    }

    static String named(int n) {
        String label;
        if (n > 0) System.out.println("positive");
        else System.out.println("not positive");
        return label;
    }

    static int fine(int n) {
        int a;
        if (n > 0) a = 1;
        int b;
        if (n > 0) b = 1; else b = 2;
        int c;
        switch (n) {
            case 1: c = 1; break;
            default: c = 0;
        }
        return b + c + (n > 0 ? a : 0);
    }

    public static void main(String[] args) {
        System.out.println(plain() + named(1).length() + fine(2));
    }
}
