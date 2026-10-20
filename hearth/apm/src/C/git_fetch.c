#define _POSIX_C_SOURCE 200809L // access under -std=c11
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "H/git_fetch.h"
#include "H/fsutil.h"
#include "H/hub/proc.h"

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

// Runs git without a shell and without prompts; true when it exits 0.
static int git(char *const argv[], const char *allow_protocols) {
    char allow[64], *out;
    snprintf(allow, sizeof(allow), "GIT_ALLOW_PROTOCOL=%s", allow_protocols);
    const char *env[] = { "GIT_TERMINAL_PROMPT=0", allow, NULL };
    int rc = proc_run(argv, NULL, 0, &out, 1 << 20, env);
    free(out);
    return rc == 0;
}

int git_fetch_is_commit(const char *s) {
    if (strlen(s) != 40) return 0;
    for (int i = 0; i < 40; i++) if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return 0;
    return 1;
}

int git_fetch_head(const char *dir, char *sha, int sha_size) {
    sha[0] = '\0';
    char dotgit[1100];
    snprintf(dotgit, sizeof(dotgit), "%s/.git", dir);
    if (access(dotgit, F_OK) != 0) return 0;              // not a checkout of its own: git would answer for the project around it
    char *argv[] = { "git", "-C", (char *)dir, "rev-parse", "--verify", "--quiet", "HEAD", NULL };
    if (!fsutil_capture(argv, sha, sha_size)) { sha[0] = '\0'; return 0; }
    return git_fetch_is_commit(sha);
}

static int checkout_commit_alone(const char *url, const char *dest_dir, const char *commit, const char *proto) {
    char *init[] = { "git", "init", "--quiet", "--", (char *)dest_dir, NULL };
    char *remote[] = { "git", "-C", (char *)dest_dir, "remote", "add", "origin", (char *)url, NULL };
    char *fetch[] = { "git", "-C", (char *)dest_dir, "fetch", "--quiet", "--depth", "1", "origin", (char *)commit, NULL };
    char *checkout[] = { "git", "-C", (char *)dest_dir, "-c", "advice.detachedHead=false", "checkout", "--quiet", "FETCH_HEAD", NULL };
    return git(init, proto) && git(remote, proto) && git(fetch, proto) && git(checkout, proto);
}

int git_fetch_repo(const char *owner, const char *repo, const char *commit, const char *ref, const char *dest_dir, char *sha, int sha_size) {
    int commit_given = commit && commit[0], ref_given = ref && ref[0];
    if (!is_safe_component(owner, (int)strlen(owner)) || !is_safe_component(repo, (int)strlen(repo))
        || (ref_given && !is_safe_component(ref, (int)strlen(ref))) || (commit_given && !git_fetch_is_commit(commit))) {
        fprintf(stderr, "apm: refusing a malformed repository reference '%s/%s'\n", owner, repo);
        return 0;
    }
    const char *base = getenv("APM_GIT_BASE");
    int mirror = base && (strncmp(base, "https://", 8) == 0 || strncmp(base, "file://", 7) == 0);
    char url[1100];
    snprintf(url, sizeof(url), "%s/%s/%s.git", mirror ? base : "https://github.com", owner, repo);
    const char *proto = mirror ? "https:file" : "https";

    int ok;
    if (commit_given) { // exactly that commit, whatever the tag or branch does later
        ok = checkout_commit_alone(url, dest_dir, commit, proto);
        if (!ok) {
            // the server does not give out a commit that no ref names: take the whole history and check it out
            if (!fsutil_rm_rf(dest_dir)) { fprintf(stderr, "apm: could not remove %s\n", dest_dir); return 0; }
            char *clone[] = { "git", "clone", "--quiet", "--", url, (char *)dest_dir, NULL };
            char *checkout[] = { "git", "-C", (char *)dest_dir, "-c", "advice.detachedHead=false", "checkout", "--quiet", (char *)commit, NULL };
            ok = git(clone, proto) && git(checkout, proto);
        }
    } else if (ref_given) {
        char *clone[] = { "git", "-c", "advice.detachedHead=false", "clone", "--quiet", "--depth", "1", "--branch", (char *)ref, "--", url, (char *)dest_dir, NULL };
        ok = git(clone, proto);
    } else {
        char *clone[] = { "git", "clone", "--quiet", "--depth", "1", "--", url, (char *)dest_dir, NULL };
        ok = git(clone, proto);
    }
    if (!ok) {
        fprintf(stderr, "apm: could not fetch %s/%s from git\n", owner, repo);
        fsutil_rm_rf(dest_dir);
        return 0;
    }
    if (!git_fetch_head(dest_dir, sha, sha_size)) {
        fprintf(stderr, "apm: could not read the commit of %s\n", dest_dir);
        fsutil_rm_rf(dest_dir);
        return 0;
    }
    if (commit_given && strcmp(sha, commit) != 0) {
        fsutil_rm_rf(dest_dir);
        fprintf(stderr, "apm: expected commit %.12s but got %.12s for %s/%s\n", commit, sha, owner, repo);
        return 0;
    }
    return 1;
}

int git_fetch_github(const char *spec, const char *dest_dir, const char *commit, char *sha, int sha_size) {
    char user[128], repo[128], ref[128];
    if (!parse_github_spec(spec, user, sizeof(user), repo, sizeof(repo), ref, sizeof(ref))) {
        fprintf(stderr, "apm: malformed github spec '%s' (expected github:user/repo or github:user/repo@ref)\n", spec);
        return 0;
    }
    return git_fetch_repo(user, repo, commit, ref, dest_dir, sha, sha_size);
}
