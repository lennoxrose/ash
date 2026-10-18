package t;

sealed interface Animal permits Dog, Cat { }
record Dog(String name) implements Animal { }
record Cat(int lives) implements Animal { }

public class RunFeatures {
    static String speak(Animal a) {
        return switch (a) {
            case Dog d -> d.name() + " barks";
            case Cat c -> "cat with " + c.lives();
        };
    }

    static <T> T firstOf(T a, T b) {
        return a;
    }

    static int bucket(int n) {
        switch (n) {
            case 1:
            case 2:
                return 10;
            case 3:
                return 30;
            default:
                return -1;
        }
    }

    public static void main(String[] args) {
        System.out.println(speak(new Dog("rex")) + " / " + speak(new Cat(9)));
        var greeting = firstOf("hi", "yo");
        System.out.println(greeting + " " + bucket(2) + " " + bucket(3) + " " + bucket(9));
        int n = 7;
        if (n > 10) {
            System.out.println("big");
        } else if (n > 5) {
            System.out.println("medium");
        } else {
            System.out.println("small");
        }
        int k = 0;
        while (true) {
            k++;
            if (k == 4) {
                break;
            }
        }
        System.out.println(k);
    }
}
