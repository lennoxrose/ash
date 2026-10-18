package ash.json;

import ash.util.List;
import ash.util.ArrayList;
import ash.util.Map;
import ash.util.HashMap;

/// JSON text <-> Object trees: Map<String, Object>, List<Object>, String, numbers, Boolean, null.
public final class Json {
    private final String text;
    private int pos;

    private Json(String text) {
        this.text = text;
        this.pos = 0;
    }

    /// Parses text; throws ParseException when malformed.
    public static Object? parse(String text) throws ParseException {
        Json p = new Json(text);
        p.skip();
        Object? v = p.value();
        p.skip();
        if (p.pos < text.length()) {
            throw new ParseException("trailing characters at " + p.pos);
        }
        return v;
    }

    /// JSON text for a tree of maps, lists, strings, numbers, booleans and null.
    public static String stringify(Object? v) {
        StringBuilder sb = new StringBuilder();
        write(sb, v);
        return sb.toString();
    }

    private static void write(StringBuilder sb, Object? v) {
        if (v == null) {
            sb.append("null");
        } else if (v instanceof String s) {
            sb.append(quote(s));
        } else if (v instanceof Boolean b) {
            sb.append(String.valueOf(b));
        } else if (v instanceof List l) {
            sb.append("[");
            for (int i = 0; i < l.size(); i++) {
                if (i > 0) {
                    sb.append(",");
                }
                write(sb, l.get(i));
            }
            sb.append("]");
        } else if (v instanceof Map m) {
            sb.append("{");
            bool first = true;
            for (Object k : m.keys()) {
                if (!first) {
                    sb.append(",");
                }
                first = false;
                sb.append(quote(String.valueOf(k))).append(":");
                write(sb, m.get(k));
            }
            sb.append("}");
        } else {
            sb.append(String.valueOf(v));
        }
    }

    private static String quote(String s) {
        return "\"" + s.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
    }

    private void skip() {
        while (pos < text.length() && text.charAt(pos) <= 32) {
            pos++;
        }
    }

    private Object? value() throws ParseException {
        if (pos >= text.length()) {
            throw new ParseException("unexpected end of input");
        }
        char c = text.charAt(pos);
        if (c == '{') {
            return object();
        }
        if (c == '[') {
            return array();
        }
        if (c == '"') {
            return string();
        }
        if (text.startsWith("true", pos)) {
            pos += 4;
            return true;
        }
        if (text.startsWith("false", pos)) {
            pos += 5;
            return false;
        }
        if (text.startsWith("null", pos)) {
            pos += 4;
            return null;
        }
        return number();
    }

    private Object number() throws ParseException {
        int start = pos;
        while (pos < text.length() && (text.charAt(pos) == '-' || text.charAt(pos) == '.' || (text.charAt(pos) >= '0' && text.charAt(pos) <= '9'))) {
            pos++;
        }
        if (start == pos) {
            throw new ParseException("unexpected character at " + pos);
        }
        return Double.parse(text.substring(start, pos));
    }

    private String string() throws ParseException {
        pos++;
        StringBuilder sb = new StringBuilder();
        while (pos < text.length() && text.charAt(pos) != '"') {
            char c = text.charAt(pos);
            if (c == '\\') {
                pos++;
                char e = text.charAt(pos);
                if (e == 'n') {
                    sb.append('\n');
                } else if (e == 't') {
                    sb.append('\t');
                } else {
                    sb.append(e);
                }
            } else {
                sb.append(c);
            }
            pos++;
        }
        if (pos >= text.length()) {
            throw new ParseException("unterminated string");
        }
        pos++;
        return sb.toString();
    }

    private List<Object?> array() throws ParseException {
        pos++;
        List<Object?> out = new ArrayList<>();
        skip();
        if (text.charAt(pos) == ']') {
            pos++;
            return out;
        }
        while (true) {
            skip();
            out.add(value());
            skip();
            if (pos >= text.length()) {
                throw new ParseException("unterminated array");
            }
            char c = text.charAt(pos);
            pos++;
            if (c == ']') {
                return out;
            }
            if (c != ',') {
                throw new ParseException("expected ',' or ']' at " + pos);
            }
        }
    }

    private Map<String, Object?> object() throws ParseException {
        pos++;
        Map<String, Object?> out = new HashMap<>();
        skip();
        if (text.charAt(pos) == '}') {
            pos++;
            return out;
        }
        while (true) {
            skip();
            if (pos >= text.length() || text.charAt(pos) != '"') {
                throw new ParseException("expected a string key at " + pos);
            }
            String key = string();
            skip();
            if (pos >= text.length() || text.charAt(pos) != ':') {
                throw new ParseException("expected ':' at " + pos);
            }
            pos++;
            skip();
            out.put(key, value());
            skip();
            if (pos >= text.length()) {
                throw new ParseException("unterminated object");
            }
            char c = text.charAt(pos);
            pos++;
            if (c == '}') {
                return out;
            }
            if (c != ',') {
                throw new ParseException("expected ',' or '}' at " + pos);
            }
        }
    }
}
