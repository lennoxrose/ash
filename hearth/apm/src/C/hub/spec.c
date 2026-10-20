#include <string.h>
#include "H/hub/spec.h"
#include "H/git_fetch.h"

static int all_of(const char *s, size_t n, const char *extra) {
    if (n == 0) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || strchr(extra, c);
        if (!ok) return 0;
    }
    return 1;
}

static int part(const char *s, size_t n, char *out, size_t size, const char *extra) {
    if (n >= size || !all_of(s, n, extra)) return 0;
    memcpy(out, s, n);
    out[n] = '\0';
    return 1;
}

int hubspec_parse(const char *text, HubSpec *out) {
    const char *slash = strchr(text, '/');
    if (!slash || strchr(slash + 1, '/')) return 0;
    const char *at = strchr(slash + 1, '@');
    size_t repo_len = at ? (size_t)(at - slash - 1) : strlen(slash + 1);
    out->version[0] = '\0';
    if (!part(text, (size_t)(slash - text), out->owner, sizeof(out->owner), "_.-")) return 0;
    if (!part(slash + 1, repo_len, out->repo, sizeof(out->repo), "_.-")) return 0;
    if (strcmp(out->owner, ".") == 0 || strcmp(out->owner, "..") == 0 || strcmp(out->repo, ".") == 0 || strcmp(out->repo, "..") == 0) return 0;
    return !at || part(at + 1, strlen(at + 1), out->version, sizeof(out->version), "._+-");
}

int depspec_ok(const char *text) {
    HubSpec h;
    return git_fetch_spec_ok(text) || hubspec_parse(text, &h);
}

int name_is_safe(const char *name) {
    size_t n = strlen(name);
    return n > 0 && n <= 100 && all_of(name, n, "_.-") && name[0] != '.' && name[0] != '_' && name[0] != '-';
}
