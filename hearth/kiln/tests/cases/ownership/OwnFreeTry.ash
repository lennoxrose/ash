public class OwnFreeTry {
    static int guarded(int n) {
        String s = "guard" + n;
        int[] box = new int[2];
        try {
            if (n > 2) return s.length();             // finally still reads s after the return value is computed
            box[0] = n;
        } finally {
            System.out.println("finally " + s + " " + box.length);
        }
        return -1;
    }

    static void loopy() {
        for (int i = 0; i < 3; i++) {
            String s = "loop" + i;
            try {
                if (i == 1) continue;                 // finally runs after continue
                if (i == 2) break;
                System.out.println("body " + s);
            } finally {
                System.out.println("fin " + s);
            }
        }
    }

    static int thrower(int n) {
        String s = "t" + n;
        try {
            if (n > 0) throw new RuntimeException("boom");
            return s.length();
        } catch (RuntimeException e) {
            System.out.println("caught " + s);          // s must still be alive in the handler
            return -2;
        }
    }

    static String pickCase(int k) {
        switch (k) {
            case 0: {
                String a = "zero" + k;
                return a + "!";
            }
            case 1: {
                int[] t = new int[3];
                t[0] = k;
                return "one" + t.length;
            }
            default:
                String d = "dflt" + k;
                return d;
        }
    }

    public static void main(String[] args) {
        System.out.println(guarded(5) + " " + guarded(1));
        loopy();
        System.out.println(thrower(0) + " " + thrower(1));
        System.out.println(pickCase(0) + pickCase(1) + pickCase(9));
    }
}
