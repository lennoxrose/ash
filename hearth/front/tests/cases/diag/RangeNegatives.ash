package t;

public class Main {
    public static void main(String[] args) {
        int[] a = new int[4];
        int i = -1;
        a[i] = 1;                     // E0201
        String s = "abc";
        int j = -2;
        System.out.println(s.charAt(j));      // E0205
        int n = -3;
        int[] b = new int[n];         // E0209
        System.out.println(b.length);
    }
}
