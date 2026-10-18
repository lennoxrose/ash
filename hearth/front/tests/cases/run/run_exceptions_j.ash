package t;

class AppException extends Exception {
    private final int code;

    AppException(String message, int code) {
        super(message);
        this.code = code;
    }

    AppException(String message, Throwable cause) {
        super(message, cause);
        this.code = -1;
    }

    int getCode() { return code; }
}

public class RunExceptions {
    static int divide(int a, int b) {
        return a / b;
    }

    static void fail(int n) throws AppException {
        if (n > 2) {
            throw new AppException("too big: " + n, n);
        }
    }

    static String guarded(int n) {
        try {
            fail(n);
            return "ok " + n;
        } catch (AppException e) {
            return "failed " + e.getMessage() + " code " + e.getCode();
        } finally {
            System.out.println("finally " + n);
        }
    }

    public static void main(String[] args) {
        System.out.println(guarded(1));
        System.out.println(guarded(5));
        try {
            System.out.println(divide(7, 0));
        } catch (ArithmeticException e) {
            System.out.println("arith " + e.getMessage());
        }
        System.out.println(divide(-7, 2) + " " + (-7 % 3) + " " + (7 % -3));
        try {
            int[] xs = new int[2];
            xs[5] = 1;
        } catch (IndexOutOfBoundsException e) {
            System.out.println("index");
        }
        try {
            try {
                throw new AppException("inner", 7);
            } catch (AppException e) {
                throw new IllegalStateException("outer", e);
            }
        } catch (IllegalStateException e) {
            Throwable? cause = e.getCause();
            if (cause != null) {
                System.out.println(e.getMessage() + " <- " + cause.getMessage());
            }
        }
        try {
            Integer.parse("12x");
        } catch (ParseException e) {
            System.out.println("parse failed");
        }
        try {
            Object o = "text";
            Integer bad = (Integer) o;
        } catch (ClassCastException e) {
            System.out.println("cast failed");
        } catch (RuntimeException e) {
            System.out.println("runtime");
        }
        int total = 0;
        for (int i = 0; i < 5; i++) {
            try {
                if (i == 2) { continue; }
                total += i;
            } catch (RuntimeException e) {
                total = -1;
            }
        }
        System.out.println(total);
    }
}
