package ash.desktop;

/// Widget that shows fixed text.
public class Label extends Widget {
    private final String text;

    /// Creates a new Label.
    public Label(String text) {
        this.text = text;
    }

    /// Text form of this widget alone.
    public String draw() {
        return text;
    }
}
