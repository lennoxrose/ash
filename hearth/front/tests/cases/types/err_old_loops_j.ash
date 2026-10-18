package t;

// expect: use `while (condition) { ... }`
public class OldLoops {
    public static void main(String[] args) {
        int x = 1;
        during x < 3 {
            x++;
        }
    }
}
