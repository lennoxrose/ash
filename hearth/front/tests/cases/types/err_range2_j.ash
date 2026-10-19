package t;

// expect: string index is always out of bounds here: the index is 5 and the string length is 3
// expect: string index is always out of bounds here: the index is 7 and the string length is 6
// expect: string index is always negative here (-1)
// expect: array index is always out of bounds here: the index is 3 and the array length is 2
// expect: array index is always out of bounds here: the index is 12 and the array length is 8
// expect: array index is always out of bounds here: the index is 4 and the array length is 4
public class ErrRange2 {
    static final int N = 4;
    private int[] data;
    private int[] ring = new int[N];
    static int[] shared;

    ErrRange2() { data = new int[8]; }
    void reset() { data = new int[8]; }

    int pastData() { return data[12]; }
    int pastRing() { int k = N; return ring[k]; }

    static char literal() {
        String s = "abc";
        int i = 5;
        return s.charAt(i);
    }

    static char joined() {
        String a = "abc";
        String b = a + "def";
        b += "";
        return b.charAt(7);
    }

    static char negative() {
        String s = "xyz";
        int i = 0;
        i = i - 1;
        return s.charAt(i);
    }

    static int caseSelected(int x) {
        int[] a = new int[2];
        switch (x) {
            case 3: return a[x];
            default: return 0;
        }
    }
}
