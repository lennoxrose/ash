package examples;

import ash.util.ArrayList;
import ash.util.List;

abstract class Widget {
    protected final String id;
    private final List<Widget> children = new ArrayList<>();

    protected Widget(String id) {
        this.id = id;
    }

    void add(Widget child) { children.add(child); }

    abstract String draw();

    String render(int depth) {
        String line = "  ".repeat(depth) + draw() + "\n";
        for (Widget c : children) {
            line += c.render(depth + 1);
        }
        return line;
    }
}

class Window extends Widget {
    private final String title;

    Window(String id, String title) {
        super(id);
        this.title = title;
    }

    String draw() { return "[Window " + title + "]"; }
}

class Button extends Widget {
    private int clicks = 0;
    private final String label;

    Button(String id, String label) {
        super(id);
        this.label = label;
    }

    void click() { clicks++; }

    String draw() { return "(" + label + " x" + clicks + ")"; }
}

class Label extends Widget {
    private final String text;

    Label(String id, String text) {
        super(id);
        this.text = text;
    }

    String draw() { return text; }
}

public class Desktop {
    public static void main(String[] args) {
        Window win = new Window("main", "Ash Demo");
        Button ok = new Button("ok", "OK");
        win.add(new Label("hint", "Press OK"));
        win.add(&ok);               // lent: the button is clicked below
        ok.click();
        ok.click();
        System.out.println(win.render(0).trim());
    }
}
