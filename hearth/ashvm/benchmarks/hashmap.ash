package benchmarks;

import ash.util.HashMap;

class HashMapBench {
    public static void main(String[] args) {
        HashMap<String, Integer> m = new HashMap<>();
        int i = 0;
        while (i < 50000) {
            m.put(String.valueOf(i), i * 2);
            i = i + 1;
        }
        int j = 0;
        int total = 0;
        while (j < 50000) {
            Integer? v = m.get(String.valueOf(j));
            if (v != null) {
                total = total + v;
            }
            j = j + 1;
        }
        System.out.println(total);
    }
}
