#ifndef APM_INSTALL_H
#define APM_INSTALL_H
#include "H/manifest.h"

// Installs one dependency into <project_dir>/@ash-modules/<dep->name>/ if it isn't already there, then the
// dependencies of its own ash.pkg (flat, the first one installed wins; at most 16 levels deep). The value says where it
// comes from:
//   "owner/repo[@version]"        a package linked on the APMH hub: the hub resolves it to a GitHub repository and an
//                                 exact commit, which is fetched (hub.ash-project.org, no login needed)
//   "github:user/repo[@ref]"      a git repository, no hub involved
// Anything else is refused. Dependency names come from ash.pkg files that may belong to someone else, so they must be
// plain directory names (name_is_safe). After a hub install the file .apm-source in the package directory records
// "<spec> <commit>", which `apm list` shows.
// Returns 1 if the dependency ended up installed (or already was), 0 on a real failure.
int install_dependency(const char *project_dir, const Dependency *dep);

#endif
