#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "H/git_fetch.h"
#include "H/fsutil.h"

int git_fetch_is_github_spec(const char *value) {
    return strncmp(value, "github:", 7) == 0;
}

// user/repo/ref only ever contain characters that are safe to drop
// straight into a shell command -- anything else is rejected outright
// rather than escaped, since a dependency spec is attacker-controlled
// the moment it comes from someone else's published ash.pkg.
static int is_safe_component(const char *s, int len) {
    if (len <= 0) return 0;
    for (int i = 0; i < len; i++) {
        char c = s[i];
        if (!(isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.')) return 0;
    }
    return 1;
}

static int parse_github_spec(const char *spec, char *user, int user_size, char *repo, int repo_size, char *ref, int ref_size) {
    const char *rest = spec + 7; // skip "github:"
    const char *slash = strchr(rest, '/');
    if (!slash) return 0;

    int user_len = (int)(slash - rest);
    if (!is_safe_component(rest, user_len) || user_len >= user_size) return 0;
    memcpy(user, rest, (size_t)user_len);
    user[user_len] = '\0';

    const char *repo_start = slash + 1;
    const char *at = strchr(repo_start, '@');
    int repo_len = at ? (int)(at - repo_start) : (int)strlen(repo_start);
    if (!is_safe_component(repo_start, repo_len) || repo_len >= repo_size) return 0;
    memcpy(repo, repo_start, (size_t)repo_len);
    repo[repo_len] = '\0';

    if (at) {
        int ref_len = (int)strlen(at + 1);
        if (!is_safe_component(at + 1, ref_len) || ref_len >= ref_size) return 0;
        memcpy(ref, at + 1, (size_t)ref_len);
        ref[ref_len] = '\0';
    } else {
        ref[0] = '\0';
    }
    return 1;
}

int git_fetch_spec_ok(const char *spec) {
    char user[128], repo[128], ref[128];
    return git_fetch_is_github_spec(spec) && parse_github_spec(spec, user, sizeof(user), repo, sizeof(repo), ref, sizeof(ref));
}

void git_fetch_infer_name(const char *spec, char *out, int out_size) {
    char user[128], repo[128], ref[128];
    if (git_fetch_is_github_spec(spec) && parse_github_spec(spec, user, sizeof(user), repo, sizeof(repo), ref, sizeof(ref))) {
        snprintf(out, (size_t)out_size, "%s", repo);
    } else {
        snprintf(out, (size_t)out_size, "%s", spec);
    }
}

int git_fetch_github(const char *spec, const char *dest_dir) {
    char user[128], repo[128], ref[128];
    if (!parse_github_spec(spec, user, sizeof(user), repo, sizeof(repo), ref, sizeof(ref))) {
        fprintf(stderr, "apm: malformed github spec '%s' (expected github:user/repo or github:user/repo@ref)\n", spec);
        return 0;
    }

    char url[400];
    snprintf(url, sizeof(url), "https://github.com/%s/%s.git", user, repo);
    // an argument vector, not a shell line: neither the spec nor the destination can be taken for shell syntax
    char *with_ref[] = { "git", "clone", "--quiet", "--depth", "1", "--branch", ref, "--", url, (char *)dest_dir, NULL };
    char *no_ref[] = { "git", "clone", "--quiet", "--depth", "1", "--", url, (char *)dest_dir, NULL };
    if (!fsutil_run(ref[0] != '\0' ? with_ref : no_ref)) {
        fprintf(stderr, "apm: 'git clone' failed for %s\n", spec);
        return 0;
    }
    return 1;
}
