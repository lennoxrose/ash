package collections;

import ash.util.ArrayList;
import ash.util.List;

/// List.clear() and Math.ceil.
class ListClearCeil {
    public static void main(String[] args) {
        List<Integer> xs = new ArrayList<>();
        xs.add(1);
        xs.add(2);
        xs.add(3);
        System.out.println(xs.size());
        xs.clear();
        System.out.println(xs.size());
        xs.add(9);
        System.out.println(xs.get(0));
        System.out.println(Math.ceil(2.1));
        System.out.println(Math.ceil(-2.1));
        System.out.println(Math.ceil(3.0));
        System.out.println(Math.ceil(0.0));
    }
}
