import ash.util.ArrayList;
import ash.util.List;

/// A parameter passed on to the method itself is not "kept" by that: natural recursive code borrows it, and a
/// parameter lent with &p to an overridable method is not kept either.
abstract class Painter {
    abstract int paint(Theme t);
}

class RedPainter extends Painter {
    int paint(Theme t) { return t.ink(); }
}

final class Theme {
    int ink() { return 7; }
}

final class Tree {
    public final List<Tree> kids = new ArrayList<>();
    int weight() { return 1 + kids.size(); }
}

public class OwnRecursion {
    static int depth(int[] xs, int n) {
        if (n == 0) return xs.length;
        return depth(xs, n - 1) + 1;           // xs only goes to this same method
    }

    static int paintTree(Tree n, Theme t) {
        Painter p = new RedPainter();
        int sum = p.paint(&t) + n.weight();    // t is lent to an overridable method: not kept
        for (Tree k : n.kids) sum += paintTree(k, t);
        return sum;
    }

    static int levels(Tree n, Theme t, int level) {
        if (level == 0) return n.weight() + t.ink();
        return levels(n, t, level - 1);        // both parameters go to the same method
    }

    static int[] saved;
    static void keep(int[] xs, int n) {
        if (n == 0) { saved = xs; return; }    // really stored: the guess "only read" is wrong
        keep(xs, n - 1);
    }

    public static void main(String[] args) {
        int[] a = {1, 2, 3};
        System.out.println(depth(a, 2) + depth(a, 1));          // a stays usable: depth borrows it
        Tree root = new Tree();
        root.kids.add(new Tree());
        Theme t = new Theme();
        System.out.println(paintTree(root, t) + paintTree(root, t));
        System.out.println(levels(root, t, 3) + levels(root, t, 1));
        int[] b = {9};
        keep(b, 1);
        System.out.println(saved[0]);
    }
}
