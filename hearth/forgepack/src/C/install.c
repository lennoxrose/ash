#include <stdio.h>
#include "H/install.h"
#include "H/fsutil.h"
#include "H/git_fetch.h"
#include "H/ui.h"

// GitHub-only for now, deliberately -- no registry/index yet (see
// ideas/ash_modules.md's phased plan), so a git source is the only thing
// forgepack can actually resolve without a server. path:/plain-version
// dependencies are a later addition, not scoped in yet.
int install_dependency(const char *project_dir, const Dependency *dep) {
    char dest[1024];
    snprintf(dest, sizeof(dest), "%s/@ash-modules/%s", project_dir, dep->name);

    if (fsutil_dir_exists(dest)) {
        ui_bullet("%s already installed", dep->name);
        return 1;
    }

    if (!git_fetch_is_github_spec(dep->value)) {
        ui_err("'%s' is not a github: dependency ('%s') -- only github:user/repo[@ref] is supported right now",
               dep->name, dep->value);
        return 0;
    }

    ui_info("fetching %s (%s)", dep->name, dep->value);
    if (!git_fetch_github(dep->value, dest)) return 0;
    ui_ok("%s installed", dep->name);
    return 1;
}
