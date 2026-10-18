package t;

public class RunWrap {
    static int hash(String s) {
        int h = 7;
        for (int i = 0; i < s.length(); i++) {
            h = 31 * h + s.charAt(i);
        }
        return h;
    }

    public static void main(String[] args) {
        int max = Integer.MAX_VALUE;
        int min = Integer.MIN_VALUE;
        System.out.println((max + 1) + " " + (min - 1) + " " + (max * 2) + " " + (46341 * 46341) + " " + (-min));
        int x = max;
        x++;
        int y = max;
        y += 5;
        int z = min;
        z--;
        System.out.println(x + " " + y + " " + z);
        System.out.println((1 << 31) + " " + (1 << 32) + " " + (-1 >>> 28) + " " + (5 << 30));
        int fact = 1;
        for (int i = 1; i <= 20; i++) {
            fact *= i;
        }
        System.out.println(fact);
        System.out.println(hash("The quick brown fox jumps over the lazy dog"));
        System.out.println((int) 3000000000L + " " + (int) 1e10 + " " + (int) -1e10 + " " + (byte) 200 + " " + (short) 70000 + " " + (int) (char) 65601);
        int sum = 0;
        for (int i = 0; i < 1000; i++) {
            sum += i * 4000000;
        }
        System.out.println(sum);
    }
}
