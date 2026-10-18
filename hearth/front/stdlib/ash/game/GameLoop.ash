package ash.game;

import ash.lang.function.Consumer;

/// Fixed-step loop: calls `update` with the tick number `ticks` times (no real-time clock).
public final class GameLoop {
    private int tick = 0;

    public GameLoop() { }

    /// Performs the task.
    public void run(int ticks, Consumer<Integer> update) {
        for (int i = 0; i < ticks; i++) {
            tick++;
            update.accept(tick);
        }
    }

    /// Current tick number.
    public int getTick() {
        return tick;
    }
}
