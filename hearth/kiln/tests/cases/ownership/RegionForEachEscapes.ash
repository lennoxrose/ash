// for-each loops that let something allocated in the body outlive the iteration, or that iterate in a way that
// runs project code. None may be a region.
import ash.util.ArrayList;
import ash.util.List;
import ash.util.Iterator;
import ash.util.Map;
import ash.util.HashMap;

public class RegionForEachEscapes {
    static String[] names = new String[4];
    static String latest = "";
    static List<String> shared = new ArrayList<>();
    static String[] words = {"alpha", "beta", "gamma", "delta"};
    static String[][] table = new String[2][2];

    static class Box implements Iterable<String> {
        public Iterator<String> iterator() { return shared.iterator(); }
    }

    static class Item {
        String name;
        String[] tags = new String[2];
        Item(String name) { this.name = name; }
        String getName() { return name; }
    }

    static String outerVariable() {
        String last = "";
        for (String w : words) last = w + "!";
        return last;
    }

    static String outerAppend() {
        String acc = "";
        for (String w : words) acc += w + "!";
        return acc;
    }

    static void outerArray() {
        int i = 0;
        for (String w : words) names[i++] = w + "?";
    }

    static void staticField() {
        for (String w : words) latest = w + "#";
    }

    static void outerList() {
        for (String w : words) shared.add(w + "@");
    }

    static void elementOfRow() {
        for (String[] row : table) {
            row[0] = "row" + row.length;          // the row is older than the iteration
        }
    }

    static void rowSwap() {
        for (String[] row : table) {
            String[] fresh = new String[2];
            table[0] = fresh;                      // keeps a fresh array
        }
    }

    static int callsProject(Item[] items) {
        int total = 0;
        for (Item it : items) {
            String s = it.getName() + "!";
            total += s.length();
        }
        return total;
    }

    static String fieldOfElement(Item[] items) {
        for (Item it : items) {
            it.name = it.name + "+";
        }
        return items[0].name;
    }

    static void tagsOfElement(Item[] items) {
        for (Item it : items) {
            it.tags[0] = "t" + it.name;
        }
    }

    static int userIterable() {
        int total = 0;
        for (String s : new Box()) {
            String t = s + "x";
            total += t.length();
        }
        return total;
    }

    static long innerList() {
        long total = 0;
        for (String w : words) {
            List<String> l = new ArrayList<>();
            for (int i = 0; i < 3; i++) l.add(w + i);
            for (String x : l) total += x.length();
            total += l.size();
        }
        return total;
    }

    static String lambdaKeeps() {
        String[] out = new String[1];
        for (String w : words) {
            Runnable r = () -> out[0] = w + "L";
            r.run();
        }
        return out[0];
    }

    static String tryKeeps() {
        String last = "";
        for (String w : words) {
            try {
                last = w + "T";
            } catch (RuntimeException e) {
                last = "";
            }
        }
        return last;
    }

    static String nestedKeeps() {
        String last = "";
        for (int i = 0; i < 2; i++) {
            String tag = "n" + i;
            for (String w : words) {
                last = tag + w;
            }
        }
        return last;
    }

    public static void main(String[] args) {
        System.out.println(outerVariable());
        System.out.println(outerAppend());
        outerArray();
        System.out.println(names[0] + names[3]);
        staticField();
        System.out.println(latest);
        outerList();
        System.out.println(shared.size() + shared.get(3));
        elementOfRow();
        System.out.println(table[0][0] + table[1][0]);
        rowSwap();
        System.out.println(table[0].length);
        Item[] items = {new Item("p"), new Item("qq")};
        System.out.println(callsProject(items));
        System.out.println(fieldOfElement(items));
        tagsOfElement(items);
        System.out.println(items[1].tags[0]);
        System.out.println(userIterable());
        System.out.println(innerList());
        System.out.println(lambdaKeeps());
        System.out.println(tryKeeps());
        System.out.println(nestedKeeps());
    }
}
