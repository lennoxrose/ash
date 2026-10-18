package t;

// expect: use `try { ... } catch (Exception e) { ... }`
public class OldAttempt {
    public static void main(String[] args) {
        attempt {
            int x = 1;
        } handle (e) { }
    }
}
