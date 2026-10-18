#ifndef APM_GIT_FETCH_H
#define APM_GIT_FETCH_H

// Recognizes "github:user/repo" and "github:user/repo@ref".
int git_fetch_is_github_spec(const char *value);

// Clones the given "github:..." spec into dest_dir (shells out to the
// system `git` binary -- apm has no git implementation of its own,
// same as every other package manager that supports a git source).
// user/repo/ref are validated against a strict charset before ever
// touching a shell command string, so a crafted dependency spec can't
// inject anything. Returns 1 on success.
int git_fetch_github(const char *spec, const char *dest_dir);

// Derives the package name apm should use when none was given
// explicitly ("add github:user/repo" -> "repo").
void git_fetch_infer_name(const char *spec, char *out, int out_size);

#endif
