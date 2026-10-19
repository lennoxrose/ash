package t;

import ash.util.ArrayList;
import ash.util.List;

public class Main {
    int counter;
    static int total;
    static int helper(int x) { return x; }
    void bump() { counter++; }

    public static void main(String[] args) {
        int count = 0;
        cuont++;                                  // a local, one letter transposed
        Strng s = "a";                            // a type
        List<String> xs = new ArrayLst<>();       // an imported type
        xs.ad("x");                               // a method of the receiver's type
        xs.sze();
        xs.size;                                  // a method used as a field
        helpr(3);                                 // an unqualified call
        Math.sqroot(4.0);                         // a static method
        Main m = new Main();
        m.countr = 3;                             // a field
        m.bumb();
        int q = totl;                             // a static field
        int z = Count;                            // only the case differs
        System.out.printn("x");
        int far = completelyUnrelated;            // nothing close: no guess
        far.length();
    }
}
