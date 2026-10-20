// try statements that can let something allocated in the iteration outlive it, or whose exception may be a project
// object. None of the loops may be a region.
import ash.util.ArrayList;
import ash.util.List;

public class RegionTryEscapes {
    static String last = "";
    static List<String> messages = new ArrayList<>();
    static RuntimeException? saved;

    static class MyError extends RuntimeException {
        MyError(String m) { super(m); }
        public String getMessage() { last = "mine"; return "my:" + super.getMessage(); }
    }

    static int helper(int i) {
        if (i == 2) throw new MyError("h" + i);
        return i;
    }

    static String keepsMessage(int n) {
        for (int i = 0; i < n; i++) {
            try {
                int z = 10 / (i - 2);
            } catch (ArithmeticException e) {
                last = "msg" + e.getMessage() + i;
            }
        }
        return last;
    }

    static int keepsInList(int n) {
        for (int i = 0; i < n; i++) {
            try {
                int z = 10 / (i - 2);
            } catch (ArithmeticException e) {
                messages.add(e.getMessage() + i);
            }
        }
        return messages.size();
    }

    static void remember(RuntimeException x) { saved = x; }

    static String keepsException(int n) {
        for (int i = 0; i < n; i++) {
            String s = "k" + i;
            try {
                int z = s.length() / (i - 2);
            } catch (RuntimeException e) {
                remember(e);
                break;
            }
        }
        RuntimeException? s = saved;
        if (s == null) return "none";
        return "" + s.getMessage();
    }

    static int throwsInside(int n) {
        int total = 0;
        for (int i = 0; i < n; i++) {
            String s = "t" + i;
            try {
                if (i == 2) throw new MyError(s);
                total += s.length();
            } catch (MyError e) {
                total += ("" + e.getMessage()).length();
            }
        }
        return total + (last.isEmpty() ? 0 : 1000);
    }

    static int callsProject(int n) {
        int total = 0;
        for (int i = 0; i < n; i++) {
            String s = "p" + i;
            try {
                total += helper(i) + s.length();
            } catch (MyError e) {
                total += ("" + e.getMessage()).length();
            }
        }
        return total + (last.isEmpty() ? 0 : 1000);
    }

    static String rethrows(int n) {
        try {
            for (int i = 0; i < n; i++) {
                String s = "r" + i;
                try {
                    int z = s.length() / (i - 2);
                } catch (ArithmeticException e) {
                    throw new RuntimeException(s + e.getMessage());
                }
            }
        } catch (RuntimeException e) {
            return "" + e.getMessage();
        }
        return "none";
    }

    static String finallyKeeps(int n) {
        for (int i = 0; i < n; i++) {
            try {
                String s = "f" + i;
            } finally {
                last = "fin" + i;
            }
        }
        return last;
    }

    public static void main(String[] args) {
        System.out.println(keepsMessage(5));
        System.out.println(keepsInList(5) + messages.get(0));
        System.out.println(keepsException(5));
        System.out.println(throwsInside(5));
        last = "";
        System.out.println(callsProject(5));
        System.out.println(rethrows(5));
        System.out.println(finallyKeeps(4));
    }
}
