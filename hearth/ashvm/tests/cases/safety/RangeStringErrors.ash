public class RangeStringErrors {
    static final int SIZE = 4;
    private int[] ring = new int[SIZE];
    private int[] cells;

    RangeStringErrors() { cells = new int[8]; }

    int ringEnd() { return ring[SIZE]; }
    int cellsEnd() { return cells[8]; }

    static char tooFar() {
        String s = "abc";
        return s.charAt(3);
    }

    static char joined() {
        String s = "ab" + "cd";
        s += "e";
        return s.charAt(5);
    }

    static int selected(int x) {
        int[] a = new int[2];
        switch (x) {
            case 5, 6: return a[x];
            default: return 0;
        }
    }

    public static void main(String[] args) {
        System.out.println(tooFar() + joined() + selected(1));
    }
}
