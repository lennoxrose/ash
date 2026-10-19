package t;

import ash.util.ArrayList;
import ash.util.List;

public class Main {
    record P(int x, int y) { }
    static String name(int i) { return i == 0 ? "zero" : "n" + i; }

    public static void main(String[] args) {
        int x = 5;
        String s = "hi";
        char c = 'q';
        double d = 2.0;
        System.out.println("x = ${x}");
        System.out.println("${x}");
        System.out.println("${x}${x}");
        System.out.println("a${x + 1}b${s}c");
        System.out.println("${s.length()} ${s + "!"} ${c} ${d} ${x > 3 ? "big" : "small"}");
        System.out.println("call: ${name(0)} and ${name(7)}");
        System.out.println("nested: ${"in ${x}"}");
        System.out.println("braces: ${new P(1, 2)} ${ new int[]{1, 2, 3}.length }");
        List<Integer> xs = new ArrayList<>();
        xs.add(3);
        System.out.println("list: ${xs.get(0)} ${xs.size()}");
        System.out.println("escaped: \${x} $x $ {x} $");
        String t = "total=" + "${x}" + 1;
        System.out.println(t);
        int n = 0;
        for (int i = 0; i < 3; i++) { String u = "i=${i}"; n += u.length(); }
        System.out.println(n);
    }
}
