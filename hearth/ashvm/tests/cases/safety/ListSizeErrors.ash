import ash.util.ArrayList;
import ash.util.List;

public class ListSizeErrors {
    static int emptyGet() {
        List<Integer> l = new ArrayList<>();
        return l.get(0);
    }

    static int afterAdds() {
        ArrayList<Integer> l = new ArrayList<>();
        l.add(1);
        l.add(2);
        return l.get(2);
    }

    static void setPast() {
        List<String> l = new ArrayList<>();
        l.add("a");
        l.set(1, "b");
    }

    static int afterRemove() {
        List<Integer> l = new ArrayList<>();
        l.add(1);
        l.add(2);
        l.removeLast();
        return l.get(1);
    }

    static int negative() {
        List<Integer> l = new ArrayList<>();
        l.add(1);
        return l.get(-1);
    }

    static int viaSize() {
        List<Integer> l = new ArrayList<>();
        l.add(7);
        return l.get(l.size());
    }

    static int bothBranches(bool c) {
        List<Integer> l = new ArrayList<>();
        if (c) { l.add(1); } else { l.add(2); l.add(3); }
        return l.get(2);
    }

    static int reassigned() {
        List<Integer> l = new ArrayList<>();
        l.add(1);
        l = new ArrayList<>();
        return l.get(0);
    }

    public static void main(String[] args) {
        System.out.println("unreachable");
    }
}
