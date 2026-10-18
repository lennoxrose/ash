package benchmarks;

class LoopVarbound {
    public static void main(String[] args) {
        int n = 20000000;
        int sum = 0;
        int i = 0;
        while (i < n) {
            sum = sum + i;
            i = i + 1;
        }
        System.out.println(sum);
    }
}
