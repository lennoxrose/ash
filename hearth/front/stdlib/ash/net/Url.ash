package ash.net;

/// Parsed `scheme://host[:port]/path`. Pure text handling: the VM has no sockets.
public record Url(String scheme, String host, int port, String path) {
    /// Parses text; throws ParseException when malformed.
    public static Url parse(String text) throws ParseException {
        int sep = text.indexOf("://");
        if (sep < 0) {
            throw new ParseException("missing scheme in " + text);
        }
        String scheme = text.substring(0, sep);
        String rest = text.substring(sep + 3);
        String path = "/";
        int slash = rest.indexOf("/");
        if (slash >= 0) {
            path = rest.substring(slash);
            rest = rest.substring(0, slash);
        }
        int port = scheme.equals("https") ? 443 : 80;
        int colon = rest.indexOf(":");
        if (colon >= 0) {
            port = Integer.parse(rest.substring(colon + 1));
            rest = rest.substring(0, colon);
        }
        return new Url(scheme, rest, port, path);
    }
}
