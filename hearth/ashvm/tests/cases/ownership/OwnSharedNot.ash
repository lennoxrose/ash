class Label {
    private String text = "";
    void setText(String s) { text = s; }
}

class Column {
    private Label last;
    void add(Label l) { last = l; }
}

/// Without `shared` a stored local is moved and using it afterwards is still an error.
public class OwnSharedNot {
    public static void main(String[] args) {
        Column col = new Column();
        Label status = new Label();
        col.add(status);
        status.setText("late");
    }
}
