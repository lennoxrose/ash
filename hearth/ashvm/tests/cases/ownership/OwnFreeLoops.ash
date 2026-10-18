public class OwnFreeLoops {
    static int classify(int i) {
        switch (i % 4) {
            case 0:
                String z = "zero" + i;
                return z.length();
            case 1: {
                int[] t = new int[i % 5 + 1];
                return t.length;
            }
            default:
                return -1;
        }
    }

    static String nested(int n) {
        String out = "";
        for (int i = 0; i < n; i++) {
            String row = "r" + i;
            int[] cells = new int[3];
            for (int j = 0; j < 3; j++) {
                String cell = row + "c" + j;
                if (j == 1) continue;
                cells[j] = cell.length();
                switch (j) {
                    case 2:
                        if (i == 1) break;
                        out += cell;
                        break;
                    default:
                        out += "-";
                }
            }
            if (i == 2) break;
            out += cells[0] + ";";
        }
        return out;
    }

    static int caught(int n) {
        String a = "a" + n;
        try {
            String b = a + "b";
            if (n == 1) throw new RuntimeException(b);
            return b.length();
        } catch (RuntimeException e) {
            String c = a + "c";
            return c.length() + 1;
        } finally {
            String d = a + "d";
            System.out.println("fin " + d);
        }
    }

    static int whileLoop() {
        int total = 0;
        int i = 0;
        while (i < 5) {
            String s = "w" + i;
            i++;
            if (s.length() > 100) continue;
            do {
                int[] k = new int[2];
                total += k.length + s.length();
            } while (false);
        }
        return total;
    }

    public static void main(String[] args) {
        long sum = 0;
        for (int i = 0; i < 8; i++) sum += classify(i);
        System.out.println(sum);
        System.out.println(nested(5));
        System.out.println(caught(0) + " " + caught(1) + " " + caught(2));
        System.out.println(whileLoop());
        String[] words = {"p" + 1, "q" + 2, "r" + 3};
        String joined = "";
        for (String w : words) {
            String up = w + "!";
            joined += up;
        }
        System.out.println(joined + words.length);
    }
}
