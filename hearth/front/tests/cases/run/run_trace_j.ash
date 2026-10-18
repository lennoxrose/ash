package t;

import ash.concurrent.Thread;
import ash.net.Http;
import ash.util.ArrayList;
import ash.util.Collectors;
import ash.util.List;

public class RunTrace {
    static void inner() {
        throw new IllegalStateException("deep");
    }

    static void outer() {
        try {
            inner();
        } catch (IllegalStateException e) {
            throw new RuntimeException("wrapped", e);
        }
    }

    public static void main(String[] args) {
        try {
            outer();
        } catch (RuntimeException e) {
            e.printStackTrace();
        }
        List<String> words = new ArrayList<>();
        words.add("b");
        words.add("a");
        System.out.println(words.stream().sorted((x, y) -> x.compareTo(y)).collect(Collectors.joining("+")) + " " + words.stream().collect(Collectors.counting()));
        List<Integer> log = new ArrayList<>();
        Thread th = new Thread(() -> log.add(7));
        th.start();
        th.join();
        System.out.println(log.size() + " " + th.isFinished());
        try {
            Http.get("http://example.org");
        } catch (IOException e) {
            System.out.println(e.getMessage());
        }
        throw new IllegalArgumentException("fatal");
    }
}
