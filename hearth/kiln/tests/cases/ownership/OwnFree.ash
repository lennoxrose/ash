public class OwnFree {
    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 300000; i++) {
            String s = "item-" + i;              // fresh String, sole owner: freed at the end of each iteration
            int[] xs = new int[16];              // fresh array, sole owner
            xs[3] = i;
            String t = s + "!";
            t += s;                              // reassigned with a fresh value: the old one is freed first
            if (i % 100000 == 0) {
                System.out.println(t);
                continue;                        // continue frees what the iteration owns
            }
            total += s.length() + xs.length + t.length() + xs[3] % 7;
        }
        System.out.println(total);
    }
}
