package examples;

import ash.json.Json;
import ash.util.List;
import ash.util.Map;

public class JsonDemo {
    public static void main(String[] args) {
        String text = "{\"user\": \"ash\", \"scores\": [3, 5, 8], \"admin\": false}";
        try {
            Object? doc = Json.parse(text);
            if (doc instanceof Map m) {
                System.out.println(m.get("user") + " " + m.get("admin"));
                Object? scores = m.get("scores");
                if (scores instanceof List l) {
                    System.out.println(l.size() + " scores, last " + l.get(2));
                }
                System.out.println(Json.stringify(m));
            }
        } catch (ParseException e) {
            System.out.println("bad json: " + e.getMessage());
        }
    }
}
