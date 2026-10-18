public class FreeIt {
    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 100000; i++) {
            String s = "x" + i;
            total += s.length();
            System.free(s);
        }
        System.out.println(total);
    }
}
