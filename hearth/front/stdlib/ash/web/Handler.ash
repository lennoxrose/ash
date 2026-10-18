package ash.web;

/// Handles one request.
public interface Handler {
    /// Produces the response for a request.
    Response handle(Request request);
}
