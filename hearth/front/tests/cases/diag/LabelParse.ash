package t;

public class Main {
    public static void main(String[] args) {
        int n = 0;
        outer:
        {
            n++;
            break outer;
        }
    }
}
