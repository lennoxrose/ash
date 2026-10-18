package t;

public class RunFloat {
    public static void main(String[] args) {
        float a = 1.1f;
        float b = a * 3;
        float c = 0.1f + 0.2f;
        float d = 16777216f + 1f;
        float e = 1f / 3f;
        float f = (float) Math.sqrt(2.0);
        float g = 100f / 7f;
        float h = 3.4e38f;
        float i = 1.0e-5f;
        System.out.println(a);
        System.out.println(b);
        System.out.println(c);
        System.out.println(d);
        System.out.println(e);
        System.out.println(f);
        System.out.println(g);
        System.out.println(h);
        System.out.println(i);
        float j = 1.4e-45f;
        float k = 3.4028235e38f;
        System.out.println(j);
        System.out.println(k);
        System.out.println("v=" + a + " " + (double) a);
    }
}
