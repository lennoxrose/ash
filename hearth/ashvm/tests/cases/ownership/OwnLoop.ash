public class OwnLoop {
    public static void main(String[] args) {
        String a = "hello";
        String keep = "";
        for (int i = 0; i < 3; i++) {
            keep = a;                     // error: moved in the previous iteration
        }
        System.out.println(keep);
    }
}
