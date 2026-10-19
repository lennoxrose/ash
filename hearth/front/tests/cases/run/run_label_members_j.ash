// Labeled loops in static initializers, constructors, interface default methods, generic methods and enum methods.
import ash.util.List;
import ash.util.ArrayList;

public class run_label_members_j {
    static int sInit;
    static {
        outer:
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (j == 2) continue outer;
                if (i == 3) break outer;
                sInit += i * 10 + j;
            }
        }
    }

    int inst;
    run_label_members_j(int n) {
        loop:
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (i + j > 3) break loop;
                inst += i + j;
            }
        }
    }

    interface Walker {
        default int walk(int n) {
            int t = 0;
            a:
            for (int i = 0; i < n; i++) {
                b:
                for (int j = 0; j < n; j++) {
                    if (j > i) continue a;
                    if (i == 4) break a;
                    t += j;
                }
            }
            return t;
        }
    }
    static class W implements Walker { }

    static <T> int count(List<T> xs, List<T> ys) {
        int n = 0;
        outer:
        for (T x : xs) {
            for (T y : ys) {
                if (x == y) continue outer;
                n++;
            }
        }
        return n;
    }

    enum E {
        A, B;
        int f() {
            int n = 0;
            o:
            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 5; j++) {
                    if (j == ordinal() + 1) continue o;
                    if (i == 3) break o;
                    n++;
                }
            }
            return n;
        }
    }

    public static void main(String[] args) {
        System.out.println(sInit);
        System.out.println(new run_label_members_j(5).inst);
        System.out.println(new W().walk(6));
        List<String> a = new ArrayList<>();
        List<String> b = new ArrayList<>();
        a.add("p"); a.add("q"); a.add("r");
        b.add("q"); b.add("z");
        System.out.println(count(a, b));
        System.out.println(E.A.f() + " " + E.B.f());
    }
}
