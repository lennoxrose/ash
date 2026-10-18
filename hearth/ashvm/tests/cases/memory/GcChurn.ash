package memory;

import ash.util.ArrayList;

/// Allocates ~1.8M short-lived strings and lists. The default VM never frees them (hundreds of MB
/// here); the experimental collector (make GC=1) keeps the footprint small. Output is identical.
class GcChurn {
    static int churn(int n) {
        ArrayList<ArrayList<String>> keep = new ArrayList<>();
        int i = 0;
        while (i < n) {
            String s = "item-" + i;
            String t = s + "-" + s;
            ArrayList<String> pair = new ArrayList<>();
            pair.add(s);
            pair.add(t);
            if (i % 1000 == 0) {
                keep.add(pair);
            }
            i += 1;
        }
        return keep.size();
    }

    public static void main(String[] args) {
        System.out.println(churn(300000));
    }
}
