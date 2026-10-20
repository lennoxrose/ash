import ash.util.ArrayList;
import ash.util.List;
import ash.lang.function.Consumer;

public class ListSizeOk {
    static void fill(List<Integer> l) { l.add(1); l.add(2); l.add(3); }

    static int viaCall() {
        List<Integer> l = new ArrayList<>();
        fill(&l);                     // the callee grows it: unknown size
        return l.get(2);
    }

    static int alias() {
        List<Integer> l = new ArrayList<>();
        List<Integer> k = l;          // l moves into k: from here on only k is used
        k.add(5);
        return k.get(0);
    }

    static int lambda() {
        List<Integer> l = new ArrayList<>();
        Consumer<Integer> add = x -> l.add(x);
        add.accept(4);
        return l.get(0);
    }

    static int loop(int n) {
        List<Integer> l = new ArrayList<>();
        for (int i = 0; i < n; i++) l.add(i);
        if (l.isEmpty()) return -1;
        return l.get(0);
    }

    static int loopRemove() {
        List<Integer> l = new ArrayList<>();
        l.add(1); l.add(2); l.add(3);
        while (l.size() > 1) l.removeLast();
        return l.get(0);
    }

    static int oneBranch(bool c) {
        List<Integer> l = new ArrayList<>();
        if (c) l.add(1);
        if (l.size() > 0) return l.get(0);
        return 0;
    }

    static int otherMethod() {
        List<Integer> l = new ArrayList<>();
        l.add(1);
        List<Integer> part = l.subList(0, 1);   // not a method we follow: the size is not tracked after it
        l.add(9);
        return part.get(0) + l.get(1);
    }

    static int tryAdd() {
        List<Integer> l = new ArrayList<>();
        try {
            if (l.size() == 0) throw new RuntimeException("x");
            l.add(1);
        } catch (RuntimeException e) {
            l.add(2);
        } finally {
            l.add(3);
        }
        return l.get(0);
    }

    static int afterFinally() {
        List<Integer> l = new ArrayList<>();
        l.add(1);
        l.add(2);
        try {
            l.removeLast();
            l.removeLast();
        } finally {
            l.add(5);
        }
        return l.get(0);
    }

    static int catchSeesIt() {
        List<Integer> l = new ArrayList<>();
        try {
            int z = l.get(0);
        } catch (Exception e) {
            return -2;
        }
        return 0;
    }

    static int finallyUnknown(bool c) {          // the finally also runs when the try failed before removeLast
        List<Integer> l = new ArrayList<>();
        l.add(1);
        int r = 0;
        try {
            try {
                if (c) throw new RuntimeException("x");
                l.removeLast();
            } finally {
                r = l.get(0);
            }
        } catch (RuntimeException e) {
            r += 100;
        }
        return r;
    }

    static int inRange() {
        List<Integer> l = new ArrayList<>();
        l.add(10); l.add(20); l.add(30);
        l.set(2, 31);
        int a = l.remove(0);
        return a + l.get(0) + l.get(1);
    }

    public static void main(String[] args) {
        System.out.println(viaCall());
        System.out.println(alias());
        System.out.println(lambda());
        System.out.println(loop(3));
        System.out.println(loopRemove());
        System.out.println(oneBranch(true) + oneBranch(false));
        System.out.println(otherMethod());
        System.out.println(tryAdd());
        System.out.println(afterFinally());
        System.out.println(catchSeesIt());
        System.out.println(finallyUnknown(true));
        System.out.println(inRange());
    }
}
