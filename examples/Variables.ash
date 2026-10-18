package examples;

public class Variables {
    private static final int LIMIT = 3;

    public static void main(String[] args) {
        int count = 7;
        long big = 5000000000L;
        double ratio = count / 2.0;
        bool ready = count > LIMIT;
        char grade = 'B';
        String name = "Ash";
        var inferred = count * 2;
        final String greeting = "Hi " + name;
        String? maybe = ready ? "set" : null;
        System.out.println(count + " " + big + " " + ratio + " " + ready + " " + grade);
        System.out.println(greeting + " " + inferred + " " + (maybe == null ? "none" : maybe));
        System.out.println(7 / 2 + " " + (-7 / 2) + " " + (-7 % 3));
    }
}
