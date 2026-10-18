package benchmarks;

class Countdown {
    public static void main(String[] args) {
        int i = 20000000;
        int count = 0;
        while (i > 0) {
            count = count + 1;
            i = i - 1;
        }
        System.out.println(count);
    }
}
