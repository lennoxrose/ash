// What stays an error: a variable declared outside the loop that is moved inside it, and a catch variable
// moved twice in the same iteration.
public class OwnLoopFreshStillMoved {
    static ArithmeticException keptEx;
    static int keepEx(ArithmeticException e) { keptEx = e; return 1; }
    static int boom(int i) { return 10 / i; }

    public static void main(String[] args) {
        ArithmeticException outer = new ArithmeticException("x");
        int total = 0;
        for (int i = 0; i < 3; i++) {
            try {
                total += boom(0);
            } catch (ArithmeticException e) {
                total += keepEx(outer);       // error: `outer` is moved in the previous iteration
                total += keepEx(e);
                total += keepEx(e);           // error: `e` was moved a line above
            }
        }
        System.out.println(total);
    }
}
