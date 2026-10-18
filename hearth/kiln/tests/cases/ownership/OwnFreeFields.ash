public class OwnFreeFields {
    static class Buffer {
        int[] cells = new int[4];                 // fresh initializer, only indexed: owned
        String label;                             // only ever given fresh concatenations: owned
        String shared = "shared";                 // a literal initializer is shared: never owned
        int[] borrowed;                           // assigned from a parameter: never owned
        String leaked;                            // returned by a getter: never owned

        Buffer(int n, int[] given) {
            label = "buf" + n;
            borrowed = given;
            leaked = "leak" + n;
            cells[1] = n;
        }
        void relabel(int n) { label = label + n; cells = new int[2]; }   // reassigning an owned field frees its old value
        int total() { return cells[0] + cells[1] + label.length() + borrowed.length + shared.length(); }
        String leak() { return leaked; }          // lets the caller hold the field's value
    }

    public static void main(String[] args) {
        String kept = "";
        long sum = 0;
        for (int i = 0; i < 20000; i++) {
            int[] given = {1, 2, 3};              // moved into the Buffer, which keeps it in a field
            Buffer b = new Buffer(i, given);      // freed each iteration together with cells and label
            b.relabel(i);
            sum += b.total();
            if (i == 7) kept = b.leak();          // aliases leaked: it must survive b
        }
        System.out.println(sum + " " + kept);
    }
}
