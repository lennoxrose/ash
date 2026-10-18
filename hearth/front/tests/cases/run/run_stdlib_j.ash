package t;

import ash.io.Files;
import ash.io.Path;
import ash.json.Json;
import ash.math.Numbers;
import ash.test.Assert;
import ash.test.AssertionError;
import ash.time.Duration;
import ash.cli.Args;
import ash.util.HashSet;
import ash.util.List;
import ash.util.Map;
import ash.util.HashMap;
import ash.util.Optional;
import ash.util.Set;

public class RunStdlib {
    public static void main(String[] args) {
        Path dir = Path.of("/tmp/ash_stdlib_test_dir");
        try {
            if (!Files.exists(dir)) {
                Files.createDirectory(dir);
            }
            Path file = dir.resolve("note.txt");
            Files.writeString(file, "line1\n");
            Files.appendString(file, "line2");
            System.out.println(Files.readString(file).length() + " " + Files.exists(file));
            Files.delete(file);
            System.out.println(Files.exists(file));
            Files.readString(file);
        } catch (IOException e) {
            System.out.println("io: " + e.getMessage());
        }

        try {
            Object? doc = Json.parse("{\"name\": \"ash\", \"tags\": [1, 2.5, true, null], \"nested\": {\"k\": \"v\"}}");
            if (doc instanceof Map m) {
                System.out.println(m.get("name") + " " + m.size());
                System.out.println(Json.stringify(m.get("tags")));
                System.out.println(Json.stringify(m.get("nested")));
            }
            Json.parse("[1, 2");
        } catch (ParseException e) {
            System.out.println("json: " + e.getMessage());
        }

        Set<Integer> seen = new HashSet<>();
        System.out.println(seen.add(3) + " " + seen.add(3) + " " + seen.add(4) + " " + seen.size() + " " + seen.contains(4));
        Map<Integer, String> byId = new HashMap<>();
        byId.put(10, "ten");
        byId.put(2, "two");
        System.out.println(byId.get(10) + " " + byId.get(2) + " " + byId.containsKey(7));
        int sumKeys = 0;
        for (Integer k : byId.keys()) {
            sumKeys += k;
        }
        System.out.println(sumKeys);

        Optional<String> present = Optional.of("here");
        Optional<String> absent = Optional.empty();
        System.out.println(present.get() + " " + absent.isPresent() + " " + absent.orElse("fallback") + " " + present.map(s -> s.length()).get());

        System.out.println(Numbers.pow(2, 10) + " " + Numbers.gcd(48, -18) + " " + Numbers.clamp(15, 0, 10) + " " + Numbers.round(2.5));
        System.out.println(Duration.ofSeconds(90).plus(Duration.ofMillis(500)).millis());

        Args a = new Args(new String[]{"--mode", "fast", "run", "--verbose", "file.txt"});
        System.out.println(a.get("mode", "slow") + " " + a.has("verbose") + " " + a.get("missing", "dflt") + " " + a.positional().size());

        Assert.assertEquals("a" + 1, "a1");
        try {
            Assert.assertEquals(1, 2);
        } catch (AssertionError e) {
            System.out.println(e.getMessage());
        }
    }
}
