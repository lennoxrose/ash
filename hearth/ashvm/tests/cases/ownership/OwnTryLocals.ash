// Locals declared inside a try or catch block must not stay on the VM stack after it ends: the variables declared
// after the statement used to read the wrong slots ("str() expects a number" on the second line below).
public class OwnTryLocals {
    public static void main(String[] args) {
        int k = 5;
        try {
            String a = "x" + k;
            String c = a + "!";
            System.out.println(c);
        } catch (RuntimeException e) {
            System.out.println("no");
        }
        String b = "y" + k;
        int z = 3;
        System.out.println(b + z + k);
        for (int i = 0; i < 4; i++) {
            try {
                String t = "t" + i;
                int[] arr = new int[2];
                arr[i % 3] = i;
                System.out.println(t + arr[0]);
            } catch (RuntimeException e) {
                String m = "caught " + i;
                int len = m.length();
                System.out.println(m + len);
            }
            int after = i * 2;
            System.out.println("after " + after + k);
        }
        System.out.println(b + z + k);
    }
}
