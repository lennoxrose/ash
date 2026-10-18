public class OwnMoved {
    public static void main(String[] args) {
        String a = "hello";
        String b = a;
        System.out.println(a);            // error: a was moved
        System.out.println(b);
    }
}
