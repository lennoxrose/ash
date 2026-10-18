public class OwnBorrowConflict {
    static void both(int[] x, int[] y) { x[0] = y[0]; }

    public static void main(String[] args) {
        int[] xs = new int[2];
        both(&mut xs, &xs);               // error: exclusive and shared borrow of xs in one call
        both(&mut xs, xs);                // error: exclusive borrow plus another use
        int[] ys = &xs;                   // error: a borrow is only a call argument
    }
}
