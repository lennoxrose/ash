package ash.desktop;

import ash.util.ArrayList;
import ash.util.List;

/// Base of a retained widget tree. Rendering produces text: the VM has no windowing.
public abstract class Widget {
    private final List<Widget> children = new ArrayList<>();

    protected Widget() { }

    /// Adds a value (at the index, if given).
    public void add(Widget child) {
        children.add(child);
    }

    /// Text form of this widget alone.
    public abstract String draw();

    /// Text form of the value or widget subtree.
    public String render(int depth) {
        String out = "  ".repeat(depth) + draw() + "\n";
        for (Widget c : children) {
            out += c.render(depth + 1);
        }
        return out;
    }
}
