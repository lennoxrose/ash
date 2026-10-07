// Covers two bugs found writing the first real forgepack module (see
// CHANGELOG.md): a `local` inside a loop in an imported function used to
// be misdiagnosed as a failed top-level namespaced-constant declaration,
// and a namespaced call used as its OWN statement (not nested in an
// expression) used to fail to parse entirely.
forge sum_doubled(arr) {
    local total = 0;
    local i = 0;
    during i < len(arr) {
        local doubled = arr[i] * 2;
        total += doubled;
        i += 1;
    }
    yield total;
}

forge announce(label) {
    say "announced: " + label;
}
