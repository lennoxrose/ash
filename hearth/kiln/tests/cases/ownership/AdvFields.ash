// Array lengths of fields and aliasing: every assignment to a field anywhere in the project (a lambda, an anonymous
// class, another class, a subclass, a swap) counts, so no index here may be called "always out of bounds".
public class AdvFields {
    interface Action { void run(); }

    static class Holder {
        int[] cells;
        int[] spare = new int[3];
        Holder() { cells = new int[4]; }
        void grow() { cells = new int[16]; }
        Action later() { return () -> cells = new int[40]; }
        void swap() { int[] t = cells; cells = spare; spare = t; }
        void adopt(int[] other) { cells = other; }
    }

    static class Sub extends Holder {
        void bigger() { cells = new int[60]; }
    }

    static class Outside {
        static void poke(Holder h) { h.cells = new int[25]; }
    }

    static int[] table = new int[2];
    static class Resizer implements Action {
        public void run() { table = new int[33]; }
    }
    static void resizeTable() { table = new int[9]; }

    static int total(int[] a) { int s = 0; for (int v : a) s += v; return s; }

    public static void main(String[] args) {
        Holder h = new Holder();
        h.grow();
        h.cells[10] = 5;                         // 16 after grow()
        Action later = h.later();
        later.run();
        h.cells[39] = 6;                         // 40 after the lambda ran
        Outside.poke(h);
        h.cells[24] = 7;                         // 25
        Sub s = new Sub();
        s.bigger();
        s.cells[59] = 8;
        s.swap();                                // cells is the spare: 3 long
        s.cells[2] = 9;
        Holder h2 = new Holder();
        h2.adopt(new int[11]);                   // a parameter: unknown length
        h2.cells[10] = 1;
        resizeTable();
        table[8] = 3;
        Action named = new Resizer();
        named.run();
        table[32] = 4;
        System.out.println(total(h.cells) + " " + total(s.cells) + " " + total(h2.cells) + " " + total(table));
        System.out.println(h.cells.length + " " + s.cells.length + " " + h2.cells.length + " " + table.length);
    }
}
