package examples;

import ash.util.HashMap;
import ash.util.Map;

record Request(String method, String path) { }

record Response(int status, String body) {
    String render() {
        return status + " " + body;
    }
}

interface Handler {
    Response handle(Request request);
}

class Router {
    private final Map<String, Handler> routes = new HashMap<>();

    void register(String path, Handler h) {
        routes.put(path, h);
    }

    Response dispatch(Request r) {
        Handler? h = routes.get(r.path());
        if (h == null) {
            return new Response(404, "not found: " + r.path());
        }
        try {
            return h.handle(r);
        } catch (RuntimeException e) {
            return new Response(500, "error: " + e.getMessage());
        }
    }
}

public class Server {
    public static void main(String[] args) {
        Router router = new Router();
        router.register("/hello", req -> new Response(200, "hello via " + req.method()));
        router.register("/boom", req -> {
            throw new IllegalStateException("exploded");
        });
        String[] paths = {"/hello", "/boom", "/missing"};
        for (String p : paths) {
            System.out.println(p + " -> " + router.dispatch(new Request("GET", p)).render());
        }
    }
}
