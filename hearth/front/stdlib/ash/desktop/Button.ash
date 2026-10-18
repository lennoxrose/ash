package ash.desktop;

/// Clickable widget that counts its clicks.
public class Button extends Widget {
    private final String label;
    private int clicks = 0;

    /// Creates a new Button.
    public Button(String label) {
        this.label = label;
    }

    /// Registers one click.
    public void click() {
        clicks++;
    }

    /// Text form of this widget alone.
    public String draw() {
        return "(" + label + " x" + clicks + ")";
    }
}
