#ifndef APM_INSTALL_H
#define APM_INSTALL_H
#include "H/manifest.h"

// What apm install / add / update / remove ask of the resolver (install.c).
typedef struct {
    int locked;               // apm install --locked: change nothing that ash.lock does not already say, fail instead
    int update_all;           // apm update: look every package up again
    const char *update_name;  // apm update <name>: look this package (and what only it needs) up again
} InstallOpts;

// A dependency's value says where it comes from:
//   "owner/repo[@version]"        a package linked on the APMH hub: the hub resolves it to a GitHub repository and an
//                                 exact commit (hub.ash-project.org, no login needed)
//   "github:user/repo[@ref]"      a git repository, no hub involved
// Anything else is refused. Dependency names come from ash.pkg files that may belong to someone else, so they must be
// plain directory names (fsutil_safe_name).
//
// Makes @ash-modules match `m`: every [dependencies] / [dev-dependencies] entry and, from the ash.pkg of each
// installed package, its own [dependencies] (transitively, all of them flat in the project's @ash-modules, the
// project's own entries first; a name asked for with two different sources keeps the first and warns).
//
// ash.lock, when there is one, decides which commit a package is at: a package whose source is the one recorded
// there is checked out at the recorded commit (re-fetched if @ash-modules holds another one), however its tag or
// branch has moved. Everything else is looked up (the tip of its ref, or the default branch) and the commit it
// resolved to is recorded. On success ash.lock is rewritten when it changed, and packages the old lockfile had
// installed that nothing needs any more are removed. On failure nothing is written to ash.lock.
// Returns 1 on success.
int install_resolve(const char *project_dir, const Manifest *m, const InstallOpts *opts);

#endif
