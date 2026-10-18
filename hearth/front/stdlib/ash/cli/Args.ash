package ash.cli;

import ash.util.HashMap;
import ash.util.List;
import ash.util.ArrayList;

/// Splits arguments into `--name value` options, `--flag` switches and positional values.
public final class Args {
    private final HashMap<String, String> options = new HashMap<>();
    private final List<String> positional = new ArrayList<>();

    /// Creates a new Args.
    public Args(String[] args) {
        int i = 0;
        while (i < args.length) {
            String a = args[i];
            if (a.startsWith("--")) {
                String name = a.substring(2);
                if (i + 1 < args.length && !args[i + 1].startsWith("--")) {
                    options.put(name, args[i + 1]);
                    i++;
                } else {
                    options.put(name, "true");
                }
            } else {
                positional.add(a);
            }
            i++;
        }
    }

    /// True when the option was given.
    public bool has(String name) {
        return options.containsKey(name);
    }

    /// Element at the index (or the stored value).
    public String get(String name, String fallback) {
        String? v = options.get(name);
        if (v == null) {
            return fallback;
        }
        return v;
    }

    /// Arguments that are not options.
    public List<String> positional() {
        return positional;
    }
}
