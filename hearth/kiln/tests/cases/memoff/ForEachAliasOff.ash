import ash.util.ArrayList;
import ash.util.ConcurrentModificationException;
import ash.util.List;

// memory-safety is off here, so a list can be aliased (the ownership checker would refuse these programs): the
// for-each must keep its size check whenever the body can reach the walked list through another name.
public class ForEachAliasOff {
    static List<Integer> kept;
    static void keep(List<Integer> l) { kept = l; }

    static void aliasLocal() {
        List<Integer> a = new ArrayList<>();
        a.add(1); a.add(2);
        List<Integer> b = a;
        int seen = 0;
        try {
            for (int x : a) { seen++; b.add(x); }
            System.out.println("aliasLocal: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("aliasLocal: CME after " + seen);
        }
    }

    static void handedOver() {
        List<Integer> a = new ArrayList<>();
        a.add(1); a.add(2);
        keep(a);
        int seen = 0;
        try {
            for (int x : a) { seen++; kept.add(x); }
            System.out.println("handedOver: no exception");
        } catch (ConcurrentModificationException e) {
            System.out.println("handedOver: CME after " + seen);
        }
    }

    static void viaSubList() {
        List<Integer> a = new ArrayList<>();
        a.add(1); a.add(2); a.add(3);
        List<Integer> view = a.subList(0, 2);
        int seen = 0;
        try {
            for (int x : a) { seen++; view.add(x); }
            System.out.println("viaSubList: no exception, size " + a.size());
        } catch (ConcurrentModificationException e) {
            System.out.println("viaSubList: CME after " + seen);
        }
    }

    public static void main(String[] args) {
        aliasLocal();
        handedOver();
        viaSubList();
    }
}
