package benchmarks;

class Loop {
    public static void main(String[] args) {
        int sum = 0;
        int i = 0;
        while (i < 20000000) {
            sum = sum + i;
            i = i + 1;
        }
        System.out.println(sum);
    }
}
