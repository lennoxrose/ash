public class OwnBranch {
    public static void main(String[] args) {
        String a = "hello";
        String keep = "";
        if (args.length > 0) keep = a;    // moved on one path only
        System.out.println(a);            // error: maybe moved
        System.out.println(keep);
    }
}
