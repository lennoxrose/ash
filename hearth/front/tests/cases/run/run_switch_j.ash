package t;

public class RunSwitch {
    static String fall(int n) {
        String out = "";
        switch (n) {
            case 1:
                out += "one,";
            case 2:
                out += "two,";
                break;
            case 3:
                out += "three,";
            default:
                out += "dflt,";
            case 4:
                out += "four,";
        }
        return out;
    }

    static int early(int n, bool flag) {
        int r = 0;
        switch (n) {
            case 5:
                r += 1;
                if (flag) {
                    break;
                }
                r += 10;
                break;
            default:
                r = -1;
        }
        return r;
    }

    static String word(String s) {
        switch (s) {
            case "a":
            case "b":
                return "ab";
            case "c":
                return "c";
        }
        return "other";
    }

    public static void main(String[] args) {
        System.out.println(fall(1) + " " + fall(2) + " " + fall(3) + " " + fall(4) + " " + fall(9));
        System.out.println(early(5, true) + " " + early(5, false) + " " + early(0, false));
        System.out.println(word("a") + word("b") + word("c") + word("z"));
        int sum = 0;
        for (int i = 0; i < 6; i++) {
            switch (i % 3) {
                case 0:
                    continue;
                case 1:
                    sum += 1;
                case 2:
                    sum += 10;
                    break;
            }
            sum += 100;
        }
        System.out.println(sum);
    }
}
