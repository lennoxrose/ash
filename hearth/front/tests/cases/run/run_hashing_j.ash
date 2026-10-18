package t;

import ash.util.HashMap;
import ash.util.HashSet;
import ash.util.List;
import ash.util.Map;
import ash.util.Set;

class Key {
    private final int id;

    Key(int id) { this.id = id; }

    int getId() { return id; }

    @Override
    public int hashCode() { return 7; }

    @Override
    public bool equals(Object? o) {
        if (o instanceof Key k) {
            return k.getId() == id;
        }
        return false;
    }
}

public class RunHashing {
    public static void main(String[] args) {
        Map<Key, String> m = new HashMap<>();
        for (int i = 0; i < 5; i++) {
            m.put(new Key(i), "v" + i);
        }
        System.out.println(m.size() + " " + m.get(new Key(0)) + " " + m.get(new Key(3)) + " " + m.get(new Key(4)) + " " + m.get(new Key(9)));
        System.out.println(m.put(new Key(2), "two") + " " + m.get(new Key(2)) + " " + m.size());
        System.out.println(m.remove(new Key(1)) + " " + m.containsKey(new Key(1)) + " " + m.size() + " " + m.get(new Key(0)) + " " + m.get(new Key(2)));
        System.out.println(m.remove(new Key(0)) + " " + m.remove(new Key(4)) + " " + m.size() + " " + m.get(new Key(3)));
        int sum = 0;
        for (Key k : m.keys()) {
            sum += k.getId();
        }
        System.out.println(sum + " " + m.values().size());
        Set<Key> s = new HashSet<>();
        System.out.println(s.add(new Key(1)) + " " + s.add(new Key(2)) + " " + s.add(new Key(1)) + " " + s.size() + " " + s.contains(new Key(2)) + " " + s.remove(new Key(1)) + " " + s.remove(new Key(1)) + " " + s.size());
        Map<String, Integer> plain = new HashMap<>();
        plain.put("a", 1);
        plain.put("a", 2);
        plain.put("b", 3);
        System.out.println(plain.size() + " " + plain.get("a") + " " + plain.remove("a") + " " + plain.size());
    }
}
