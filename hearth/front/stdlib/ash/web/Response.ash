package ash.web;

/// An HTTP-style response value.
public record Response(int status, String body) {
    /// Text form of the value or widget subtree.
    public String render() {
        return status + " " + body;
    }
}
