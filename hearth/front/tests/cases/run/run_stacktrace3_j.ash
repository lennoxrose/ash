package t;

public class Main {
    static int get(int[] a, int i) {
        return a[i];
    }

    static int middle(int[] a) {
        return get(a, 7) + 1;
    }

    public static void main(String[] args) {
        int[] a = new int[3];
        System.out.println(middle(a));
    }
}
