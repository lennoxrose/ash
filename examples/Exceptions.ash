package examples;

class ValidationException extends Exception {
    private final String field;

    ValidationException(String field, String message) {
        super(message);
        this.field = field;
    }

    String getField() { return field; }
}

public class Exceptions {
    static int parseAge(String text) throws ValidationException {
        try {
            int age = Integer.parse(text);
            if (age < 0 || age > 150) {
                throw new ValidationException("age", "out of range: " + age);
            }
            return age;
        } catch (ParseException e) {
            throw new ValidationException("age", "not a number: " + text);
        }
    }

    static String describe(String text) {
        try {
            return "age " + parseAge(text);
        } catch (ValidationException e) {
            return e.getField() + " invalid: " + e.getMessage();
        } finally {
            System.out.println("checked " + text);
        }
    }

    public static void main(String[] args) {
        System.out.println(describe("42"));
        System.out.println(describe("abc"));
        System.out.println(describe("200"));
        try {
            int[] xs = new int[2];
            System.out.println(xs[5]);
        } catch (IndexOutOfBoundsException e) {
            System.out.println("index out of range");
        }
    }
}
