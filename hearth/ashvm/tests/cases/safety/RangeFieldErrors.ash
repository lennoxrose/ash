public class RangeFieldErrors {
    private final int[] table = new int[8];
    static final int[] PRIMES = {2, 3, 5, 7};

    int peek() {
        int i = 8;
        return table[i];
    }

    static int prime() {
        int k = 4;
        return PRIMES[k];
    }

    public static void main(String[] args) {
        System.out.println(prime() + new RangeFieldErrors().peek());
    }
}
