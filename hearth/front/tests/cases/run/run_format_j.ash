package t;
public class Main {
    public static void main(String[] args) {
        int x = 42;
        System.out.println(String.format("%d-%s", x, "a"));
        System.out.println(String.format("[%5d][%-5d][%05d][%+d][% d][%,d]", 42, 42, 42, 42, 42, 1234567));
        System.out.println(String.format("%.2f %.0f %.3f %10.2f|%-10.1f|", 3.14159, 2.5, 1.0005, -2.345, 9.99));
        System.out.println(String.format("%s %s %s %s", 1.5, 3.0, 'c', true));
        System.out.println(String.format("%c %b %x %X %o %#x %08.3f", 'z', false, 255, 255, 8, 255, 3.14159));
        System.out.println("%d%%".formatted(50));
        System.out.printf("%s=%d%n", "k", 7);
        System.out.println(String.format("%5s|%-5s|%.2s|%S", "ab", "ab", "abcdef", "up"));
        System.out.println(String.format("%.2f %f %.1f", 1.005, 0.1 + 0.2, 1e10));
        System.out.println(String.format("%x", -1));
        System.out.println(String.format("%.2f", 1e-7) + " " + String.format("%.1f", 0.05));
        long big = 123456789012L;
        System.out.println(String.format("%d %,d", big, big));
        try { String.format("%d"); } catch (IllegalArgumentException e) { System.out.println("caught " + e.getMessage()); }
        try { String.format("%q", 1); } catch (IllegalArgumentException e) { System.out.println("caught " + e.getMessage()); }
    }
}
