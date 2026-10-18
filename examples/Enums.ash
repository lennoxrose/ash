package examples;

enum Planet {
    MERCURY(3), EARTH(6), JUPITER(70);

    private final int radius;

    Planet(int radius) {
        this.radius = radius;
    }

    int getRadius() { return radius; }

    bool isGiant() { return radius > 50; }
}

public class Enums {
    static String size(Planet p) {
        return switch (p) {
            case MERCURY -> "small";
            case EARTH -> "medium";
            case JUPITER -> "huge";
        };
    }

    public static void main(String[] args) {
        for (Planet p : Planet.values()) {
            System.out.println(p + " #" + p.ordinal() + " " + size(p) + " " + p.isGiant());
        }
        Planet chosen = Planet.valueOf("EARTH");
        switch (chosen) {
            case EARTH:
                System.out.println("home");
                break;
            default:
                System.out.println("away");
        }
        System.out.println(chosen == Planet.EARTH);
    }
}
