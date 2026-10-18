package t;

import ash.concurrent.Executor;
import ash.concurrent.Future;
import ash.desktop.Button;
import ash.desktop.Label;
import ash.desktop.Widget;
import ash.game.GameLoop;
import ash.game.Vec2;
import ash.net.Url;
import ash.util.ArrayList;
import ash.util.List;
import ash.util.Result;
import ash.web.Request;
import ash.web.Response;
import ash.web.Router;

public class RunStdlib2 {
    static Result<Integer, String> half(int n) {
        if (n % 2 != 0) {
            return Result.err("odd: " + n);
        }
        return Result.ok(n / 2);
    }

    public static void main(String[] args) {
        Result<Integer, String> a = half(10);
        Result<Integer, String> b = half(7);
        System.out.println(a.isOk() + " " + a.unwrap() + " " + b.isOk() + " " + b.unwrapOr(-1) + " " + b.getError());
        System.out.println(a.map(v -> v * 3).unwrap());

        List<Integer> xs = new ArrayList<>();
        xs.add(5);
        xs.add(1);
        xs.add(3);
        List<Integer> sorted = xs.stream().sorted((p, q) -> p - q).toList();
        System.out.println(sorted.get(0) + " " + sorted.get(2) + " " + xs.stream().count() + " " + xs.stream().anyMatch(v -> v > 4));

        Executor ex = new Executor();
        Future<Integer> ok = ex.submit(() -> 6 * 7);
        Future<Integer> bad = ex.submit(() -> 1 / 0);
        System.out.println(ok.get() + " " + bad.isFailed());

        try {
            Url u = Url.parse("https://example.org:8443/a/b");
            System.out.println(u.scheme() + " " + u.host() + " " + u.port() + " " + u.path());
            Url.parse("nonsense");
        } catch (ParseException e) {
            System.out.println("url: " + e.getMessage());
        }

        Router r = new Router();
        r.register("/ping", req -> new Response(200, "pong"));
        System.out.println(r.dispatch(new Request("GET", "/ping")).render() + " | " + r.dispatch(new Request("GET", "/x")).render());

        Vec2 v = new Vec2(3.0, 4.0).plus(new Vec2(0.0, 0.0)).scale(2.0);
        System.out.println(v + " " + v.length());
        GameLoop loop = new GameLoop();
        List<Integer> seen = new ArrayList<>();
        loop.run(3, tick -> seen.add(tick * 10));
        System.out.println(seen.size() + " " + seen.get(2) + " " + loop.getTick());

        Label hint = new Label("hi");
        Button ok2 = new Button("OK");
        ok2.click();
        hint.add(ok2);
        System.out.println(hint.render(0).trim());
    }
}
