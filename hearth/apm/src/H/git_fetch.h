#ifndef APM_GIT_FETCH_H
#define APM_GIT_FETCH_H

// Recognizes "github:user/repo" and "github:user/repo@ref".
int git_fetch_is_github_spec(const char *value);

// 1 when `spec` is a well-formed "github:user/repo[@ref]" (user, repo and ref made of safe characters only).
int git_fetch_spec_ok(const char *spec);

// 1 when `s` is a full commit id: 40 lowercase hex digits.
int git_fetch_is_commit(const char *s);

// The commit a checkout in `dir` is at (git rev-parse HEAD), into `sha` (at least 41 bytes). Returns 0 when `dir` is
// not a git checkout.
int git_fetch_head(const char *dir, char *sha, int sha_size);

// Puts the repository of the given "github:..." spec into dest_dir (shells out to the system `git` binary, run with
// an argument vector and no shell -- apm has no git implementation of its own, same as every other package manager
// that supports a git source). user/repo/ref are also validated against a strict charset, so a crafted dependency
// spec can't inject anything or turn into a git option.
// Without `commit` the tip of the spec's ref (or the default branch) is cloned shallowly. With `commit` (a full id,
// from ash.lock) exactly that commit is checked out, however the ref has moved since: fetched on its own, or, when
// the server will not give out a bare commit, by a full clone. `sha` receives the commit that ended up checked out.
// Returns 1 on success.
int git_fetch_github(const char *spec, const char *dest_dir, const char *commit, char *sha, int sha_size);

// The same for a repository named by owner and repo (a package of the hub): at exactly `commit` (40 hex) when given,
// else at `ref` (a tag or branch) when given, else the head of the default branch. owner/repo/ref/commit are validated
// here too. The host comes from $APM_GIT_BASE (a mirror, or file:// in tests) and defaults to https://github.com. git
// only speaks https (and file for such an override); it never prompts. `sha` receives the commit checked out. Returns
// 1 on success; on failure dest_dir is removed.
int git_fetch_repo(const char *owner, const char *repo, const char *commit, const char *ref, const char *dest_dir, char *sha, int sha_size);

// Derives the package name apm should use when none was given
// explicitly ("add github:user/repo" -> "repo").
void git_fetch_infer_name(const char *spec, char *out, int out_size);

#endif
