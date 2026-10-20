#ifndef APM_GIT_FETCH_H
#define APM_GIT_FETCH_H

// Recognizes "github:user/repo" and "github:user/repo@ref".
int git_fetch_is_github_spec(const char *value);

// Clones the given "github:..." spec into dest_dir (runs the system `git` binary -- apm has no git implementation of
// its own, same as every other package manager that supports a git source). user/repo/ref are validated against a
// strict charset and git is started WITHOUT a shell, so a crafted dependency spec can't inject anything. Returns 1 on success.
int git_fetch_github(const char *spec, const char *dest_dir);

// Fetches https://github.com/<owner>/<repo> into dest_dir at exactly `commit` (40 hex) when given, else at `ref` (a tag or
// branch) when given, else the head of the default branch; shallow either way. owner/repo/ref/commit are validated here
// too. The host comes from $APM_GIT_BASE (a mirror, or file:// in tests) and defaults to https://github.com. git only
// speaks https (and file for such an override); it never prompts. Returns 1 on success; on failure dest_dir is removed.
int git_fetch_repo(const char *owner, const char *repo, const char *commit, const char *ref, const char *dest_dir);

// Derives the package name apm should use when none was given
// explicitly ("add github:user/repo" -> "repo").
void git_fetch_infer_name(const char *spec, char *out, int out_size);

#endif
