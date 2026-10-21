import ash.util.ArrayList;
import ash.util.List;

/// `shared` keeps a local usable after it was stored: Label status = ...; col.add(status); status.setText(...).
class Label {
    private String text = "";
    void setText(String s) { text = s; }
    String text() { return text; }
}

class Column {
    private List<Label> items = new ArrayList<>();
    void add(Label l) { items.add(l); }
    String joined() {
        String r = "";
        for (Label l : items) r += l.text() + ";";
        return r;
    }
}

public class OwnShared {
    public static void main(String[] args) {
        Column col = new Column();
        shared Label status = new Label();
        col.add(status);                       // stored in col, still usable here
        status.setText("ready");
        col.add(status);
        System.out.println(col.joined());
        System.out.println(status.text());
    }
}
