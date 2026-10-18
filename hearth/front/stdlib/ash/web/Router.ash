package ash.web;

import ash.util.HashMap;
import ash.util.Map;

/// Routes requests by exact path; unknown paths give 404 and handler exceptions give 500.
public final class Router {
    private final Map<String, Handler> routes = new HashMap<>();

    public Router() { }

    /// Handles requests for exactly this path.
    public void register(String path, Handler handler) {
        routes.put(path, handler);
    }

    /// Routes a request to its handler; 404 and 500 are produced here.
    public Response dispatch(Request request) {
        Handler? h = routes.get(request.path());
        if (h == null) {
            return new Response(404, "not found: " + request.path());
        }
        try {
            return h.handle(request);
        } catch (RuntimeException e) {
            return new Response(500, "error: " + e.getMessage());
        }
    }
}
