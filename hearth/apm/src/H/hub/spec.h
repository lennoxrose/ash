#ifndef APM_SPEC_H
#define APM_SPEC_H

// A package of the hub is named by its GitHub owner and repository, exactly as on the hub: "lennoxrose/ash-stdlib",
// optionally pinned with "@<version>" (a git tag, with or without a leading v). "github:user/repo[@ref]" (see git_fetch.h)
// stays as the way to depend on a repository that is not linked on the hub.
typedef struct {
    char owner[101];
    char repo[101];
    char version[101]; // "" = no version given
} HubSpec;

// Parses "owner/repo" or "owner/repo@version". Only [A-Za-z0-9_.-] in owner and repo (so a spec can never name a path
// outside its own directory or carry shell syntax) and [0-9A-Za-z._+-] in the version. Returns 1 on success.
int hubspec_parse(const char *text, HubSpec *out);

// A name that is safe as a directory name under @ash-modules/: [A-Za-z0-9][A-Za-z0-9_.-]*, at most 100 characters.
int name_is_safe(const char *name);

#endif
