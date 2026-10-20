// Compound assignment and ++ / -- store (T)(x op y): a byte, short or char wraps, an int drops the fraction of a
// double result (they stored the wide value: `byte b = 127; b++` gave 128, `int x = 1; x += 1.5` gave 2.5).
public class run_compound_narrow_j {
    static byte sb = 127;
    byte fb = 120;

    public static void main(String[] args) {
        int x = 1;
        x += 1.5;
        System.out.println(x);
        int y = 10;
        y *= 0.25;
        System.out.println(y);
        long big = 5;
        big += 2.9;
        System.out.println(big);
        char c = 'a';
        c += 1;
        System.out.println(c);
        char d = 'z';
        d++;
        System.out.println(d);
        char z = 0;
        z--;
        System.out.println((int) z);
        short s = 32767;
        s++;
        System.out.println(s);
        byte b = 100;
        b += 100;
        System.out.println(b);
        b = 100;
        b *= 3;
        System.out.println(b);
        b = -128;
        b--;
        System.out.println(b);
        byte[] bs = new byte[2];
        bs[0] = 127;
        bs[0]++;
        System.out.println(bs[0]);
        bs[1] = 100;
        bs[1] += 100;
        System.out.println(bs[1]);
        sb++;
        System.out.println(sb);
        run_compound_narrow_j o = new run_compound_narrow_j();
        o.fb += 10;
        System.out.println(o.fb);
        int w = 7;
        w <<= 1;
        w -= 20L;
        System.out.println(w);
        byte q = 5;
        int viaValue = (q += 125) + 1;
        System.out.println(viaValue + " " + q);
        byte r = 127;
        int post = r++;
        System.out.println(post + " " + r);
    }
}
