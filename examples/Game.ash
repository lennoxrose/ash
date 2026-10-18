package examples;

import ash.util.ArrayList;
import ash.util.List;

enum Kind { PLAYER, ENEMY }

class Entity {
    private final String name;
    private final Kind kind;
    private int hp;
    private int x;

    Entity(String name, Kind kind, int hp, int x) {
        this.name = name;
        this.kind = kind;
        this.hp = hp;
        this.x = x;
    }

    String getName() { return name; }
    Kind getKind() { return kind; }
    int getHp() { return hp; }
    int getX() { return x; }
    bool isAlive() { return hp > 0; }

    void move(int dx) { x += dx; }

    void damage(int amount) {
        hp -= amount;
        if (hp < 0) {
            hp = 0;
        }
    }
}

class World {
    private final List<Entity> entities = new ArrayList<>();
    private int tick = 0;

    void spawn(Entity e) { entities.add(e); }

    void step() {
        tick++;
        for (Entity e : entities) {
            if (!e.isAlive()) {
                continue;
            }
            if (e.getKind() == Kind.ENEMY) {
                e.move(-1);
            } else {
                e.move(1);
            }
        }
        Entity player = entities.get(0);
        for (Entity e : entities) {
            if (e.getKind() == Kind.ENEMY && e.isAlive() && e.getX() == player.getX()) {
                e.damage(5);
                player.damage(2);
            }
        }
    }

    void report() {
        System.out.println("tick " + tick);
        for (Entity e : entities) {
            System.out.println("  " + e.getName() + " hp=" + e.getHp() + " x=" + e.getX());
        }
    }
}

public class Game {
    public static void main(String[] args) {
        World w = new World();
        w.spawn(new Entity("hero", Kind.PLAYER, 10, 0));
        w.spawn(new Entity("orc", Kind.ENEMY, 8, 3));
        for (int i = 0; i < 3; i++) {
            w.step();
        }
        w.report();
    }
}
