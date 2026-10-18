package t;

public class RunLongWrap {
    static long fact(int n) {
        long r = 1;
        for (int i = 2; i <= n; i++) {
            r *= i;
        }
        return r;
    }

    static long fnv(String s) {
        long h = -3750763034362895579L;
        for (int i = 0; i < s.length(); i++) {
            h ^= s.charAt(i);
            h *= 1099511628211L;
        }
        return h;
    }

    public static void main(String[] args) {
        long max = Long.MAX_VALUE;
        long min = Long.MIN_VALUE;
        System.out.println(max + " " + min + " " + (max + 1) + " " + (min - 1) + " " + (-min) + " " + (max * 2));
        System.out.println(fact(20) + " " + fact(21) + " " + fact(25));
        System.out.println(fnv("hello") + " " + fnv("The quick brown fox"));
        long a = 123456789012345678L;
        long b = 1000000007L;
        System.out.println((a / b) + " " + (a % b) + " " + (-a / b) + " " + (-a % b) + " " + (a / -b) + " " + (max / 3) + " " + (min / 7) + " " + (max % 1000));
        System.out.println((1L << 62) + " " + (1L << 63) + " " + (-1L >>> 1) + " " + (-1L >>> 60) + " " + (123456789012345L << 20) + " " + (a >> 7) + " " + (-a >> 7) + " " + (a << 5));
        System.out.println((a & 0xFFFFL) + " " + (a | 1L) + " " + (a ^ max) + " " + (~a) + " " + (~min));
        System.out.println((int) 4294967297L + " " + (int) a + " " + (long) 1e19 + " " + (long) -1e19 + " " + (long) 3.9e15 + " " + (double) a + " " + (long) 1.5e17);
        try {
            System.out.println(Long.parse("9223372036854775807") + " " + Long.parse("-9223372036854775808") + " " + Long.parse("123") + " " + (Long.parse("9007199254740993") + 1));
        } catch (ParseException e) {
            System.out.println("parse failed");
        }
        System.out.println((max > min) + " " + (a < b) + " " + (a == 123456789012345678L) + " " + (max == max - 1) + " " + (a != b) + " " + (min < -1L) + " " + (max >= max));
        long x = max;
        x--;
        x += 10;
        long y = 9007199254740993L;
        y++;
        System.out.println(x + " " + y + " " + String.valueOf(max) + " " + Long.valueOf(min).compareTo(max));
    }
}
