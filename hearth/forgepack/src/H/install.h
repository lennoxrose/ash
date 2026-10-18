#ifndef FORGEPACK_INSTALL_H
#define FORGEPACK_INSTALL_H
#include "H/manifest.h"

// Installs one dependency into <project_dir>/@ash-modules/<dep->name>/ if
// it isn't already there. Dispatches on the dependency's value: "github:"
// -> git clone, "path:" -> recursive copy, anything else (a plain
// version/range) -> prints a note and is skipped, since there's no
// registry yet to resolve a bare version against (see
// ideas/ash_modules.md's phased plan -- that's phase 3, not phase 1).
// Returns 1 if the dependency ended up installed (or already was), 0 on
// a real failure.
int install_dependency(const char *project_dir, const Dependency *dep);

#endif
