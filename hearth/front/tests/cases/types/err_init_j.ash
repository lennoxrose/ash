package t;

// expect: variable 'a' is used before it is assigned
// expect: variable 'b' is used before it is assigned
// expect: variable 'd' is used before it is assigned
public class ErrInit {
    static int plain() {
        int a;
        return a + 1;
    }

    static String branchesThatDoNotAssign(bool f) {
        String b;
        if (f) { System.out.println("x"); } else { System.out.println("y"); }
        return b;
    }

    static int compound() {
        int d;
        d += 2;
        return d;
    }
}
