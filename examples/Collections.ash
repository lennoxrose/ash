package examples;

import ash.util.ArrayList;
import ash.util.HashMap;
import ash.util.List;
import ash.util.Map;

public class Collections {
    public static void main(String[] args) {
        List<Integer> nums = new ArrayList<>();
        for (int i = 1; i <= 10; i++) {
            nums.add(i);
        }
        List<Integer> evens = nums.stream().filter(n -> n % 2 == 0).toList();
        int sumSquares = nums.stream().map(n -> n * n).reduce(0, (a, b) -> a + b);
        System.out.println(evens.size() + " " + sumSquares);
        nums.sort((a, b) -> b - a);
        System.out.println(nums.get(0) + " " + nums.get(9) + " " + nums.subList(0, 3).size());
        Map<String, List<Integer>> groups = new HashMap<>();
        groups.put("even", new ArrayList<>());
        groups.put("odd", new ArrayList<>());
        for (int n : nums) {
            List<Integer>? bucket = groups.get(n % 2 == 0 ? "even" : "odd");
            if (bucket != null) {
                bucket.add(n);
            }
        }
        for (String k : groups.keys()) {
            List<Integer>? v = groups.get(k);
            if (v != null) {
                System.out.println(k + " " + v.size());
            }
        }
        nums.forEach(n -> System.out.println(n));
    }
}
