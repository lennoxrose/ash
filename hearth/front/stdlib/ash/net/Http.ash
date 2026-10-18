package ash.net;

/// HTTP client facade. The Ash VM has no network access, so every request fails with an
/// IOException; the API exists so networked programs type-check and can handle the failure.
public final class Http {
    /// Always throws IOException("network access is not available on this runtime").
    public static String get(String url) throws IOException {
        throw new IOException("network access is not available on this runtime: " + url);
    }
}
