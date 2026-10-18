package ash.game;

/// Immutable 2D vector.
public record Vec2(double x, double y) {
    public Vec2 plus(Vec2 o) { return new Vec2(x + o.x(), y + o.y()); }
    public Vec2 scale(double k) { return new Vec2(x * k, y * k); }
    public double length() { return Math.sqrt(x * x + y * y); }
}
