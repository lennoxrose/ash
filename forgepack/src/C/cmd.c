#define _XOPEN_SOURCE 700 // exposes PATH_MAX under -std=c11 (see ashvm/kiln's import_paths.c)
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "H/cmd.h"
#include "H/manifest.h"
#include "H/install.h"
#include "H/fsutil.h"
#include "H/git_fetch.h"
#include "H/ui.h"

static void infer_project_name(char *out, int out_size) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) { snprintf(out, (size_t)out_size, "my-project"); return; }
    char *slash = strrchr(cwd, '/');
    const char *base = slash ? slash + 1 : cwd;
    int len = (int)strlen(base);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, base, (size_t)len);
    out[len] = '\0';
}

int cmd_init(const char *project_dir, int argc, char **argv) {
    (void)argc; (void)argv;
    Manifest m;
    if (manifest_load(project_dir, &m)) {
        ui_err("%s already exists", MANIFEST_FILE);
        return 1;
    }

    char name[MANIFEST_NAME_LEN];
    infer_project_name(name, sizeof(name));
    manifest_init_default(&m, name);
    if (!manifest_save(project_dir, &m)) return 1;

    ui_ok("created %s (%s@%s)", MANIFEST_FILE, m.name, m.version);
    return 0;
}

// forgepack add github:user/repo[@ref] [--as <name>]
//
// GitHub-only for now -- forgepack has no registry/index yet (it will,
// see ideas/ash_modules.md's phased plan; ash-project.org is reserved
// for that once there's a real flat index worth serving from it), so a
// git source is the only dependency kind that's actually installable
// without a server.
int cmd_add(const char *project_dir, int argc, char **argv) {
    if (argc < 1 || !git_fetch_is_github_spec(argv[0])) {
        ui_err("usage: forgepack add github:<user>/<repo>[@ref] [--as <name>]");
        return 1;
    }
    const char *source = argv[0];
    const char *explicit_name = NULL;
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--as") == 0) explicit_name = argv[i + 1];
    }

    char name[MANIFEST_NAME_LEN];
    if (explicit_name) {
        snprintf(name, sizeof(name), "%s", explicit_name);
    } else {
        git_fetch_infer_name(source, name, sizeof(name));
    }

    Manifest m;
    if (!manifest_load(project_dir, &m)) {
        ui_err("no %s here -- run 'forgepack init' first", MANIFEST_FILE);
        return 1;
    }
    if (!manifest_set_dependency(&m, name, source)) {
        ui_err("dependency table full (max %d)", MANIFEST_MAX_DEPS);
        return 1;
    }
    if (!manifest_save(project_dir, &m)) return 1;

    char ash_modules[1024];
    snprintf(ash_modules, sizeof(ash_modules), "%s/@ash-modules", project_dir);
    fsutil_mkdir_p(ash_modules);

    Dependency *dep = manifest_find_dependency(&m, name);
    if (!install_dependency(project_dir, dep)) {
        ui_err("failed to install '%s'", name);
        return 1;
    }
    ui_ok("added %s (%s)", name, source);
    return 0;
}

int cmd_install(const char *project_dir, int argc, char **argv) {
    (void)argc; (void)argv;
    Manifest m;
    if (!manifest_load(project_dir, &m)) {
        ui_err("no %s here -- run 'forgepack init' first", MANIFEST_FILE);
        return 1;
    }

    char ash_modules[1024];
    snprintf(ash_modules, sizeof(ash_modules), "%s/@ash-modules", project_dir);
    fsutil_mkdir_p(ash_modules);

    if (m.dep_count == 0) {
        ui_info("%s has no dependencies", m.name);
        return 0;
    }

    ui_header("%s@%s", m.name, m.version);
    int ok = 1;
    for (int i = 0; i < m.dep_count; i++) {
        if (!install_dependency(project_dir, &m.deps[i])) ok = 0;
    }
    if (ok) ui_ok("done");
    else ui_err("one or more dependencies failed to install");
    return ok ? 0 : 1;
}

int cmd_list(const char *project_dir, int argc, char **argv) {
    (void)argc; (void)argv;
    Manifest m;
    if (!manifest_load(project_dir, &m)) {
        ui_err("no %s here -- run 'forgepack init' first", MANIFEST_FILE);
        return 1;
    }

    ui_header("%s@%s", m.name, m.version);
    if (m.dep_count == 0) {
        ui_bullet("(no dependencies)");
        return 0;
    }
    for (int i = 0; i < m.dep_count; i++) {
        char dest[1024];
        snprintf(dest, sizeof(dest), "%s/@ash-modules/%s", project_dir, m.deps[i].name);
        if (fsutil_dir_exists(dest)) ui_bullet("%s: %s", m.deps[i].name, m.deps[i].value);
        else ui_bullet("%s: %s (not installed)", m.deps[i].name, m.deps[i].value);
    }
    return 0;
}
