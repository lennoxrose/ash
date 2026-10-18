#define _XOPEN_SOURCE 700 // exposes PATH_MAX under -std=c11 (see ashvm/kiln's import_paths.c)
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "H/cmd.h"
#include "H/manifest.h"
#include "H/install.h"
#include "H/fsutil.h"
#include "H/git_fetch.h"
#include "H/ui.h"
#include "H/license/licenses.h"

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

// apm add github:user/repo[@ref] [--as <name>] [--dev]
//
// GitHub-only for now -- apm has no registry/index yet (it will,
// see ideas/ash_modules.md's phased plan; ash-project.org is reserved
// for that once there's a real flat index worth serving from it), so a
// git source is the only dependency kind that's actually installable
// without a server.
int cmd_add(const char *project_dir, int argc, char **argv) {
    if (argc < 1 || !git_fetch_is_github_spec(argv[0])) {
        ui_err("usage: apm add github:<user>/<repo>[@ref] [--as <name>] [--dev]");
        return 1;
    }
    const char *source = argv[0];
    const char *explicit_name = NULL;
    int dev = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--as") == 0 && i + 1 < argc) explicit_name = argv[++i];
        else if (strcmp(argv[i], "--dev") == 0) dev = 1;
    }

    char name[MANIFEST_NAME_LEN];
    if (explicit_name) {
        snprintf(name, sizeof(name), "%s", explicit_name);
    } else {
        git_fetch_infer_name(source, name, sizeof(name));
    }

    Manifest m;
    if (!manifest_load(project_dir, &m)) {
        char project_name[MANIFEST_NAME_LEN];
        infer_project_name(project_name, sizeof(project_name));
        manifest_init_default(&m, project_name);
        ui_info("no %s here, creating one", MANIFEST_FILE);
    }
    EntryList *target = dev ? &m.dev_deps : &m.deps;
    if (!entry_set(target, name, source)) {
        ui_err("dependency table full (max %d)", MANIFEST_MAX_ENTRIES);
        return 1;
    }
    if (!manifest_save(project_dir, &m)) return 1;

    char ash_modules[1024];
    snprintf(ash_modules, sizeof(ash_modules), "%s/@ash-modules", project_dir);
    fsutil_mkdir_p(ash_modules);

    Dependency *dep = entry_find(target, name);
    if (!install_dependency(project_dir, dep)) {
        ui_err("failed to install '%s'", name);
        return 1;
    }
    ui_ok("added %s (%s)", name, source);
    return 0;
}

static int install_all(const char *project_dir, EntryList *list) {
    int ok = 1;
    for (int i = 0; i < list->count; i++) {
        if (!install_dependency(project_dir, &list->items[i])) ok = 0;
    }
    return ok;
}

int cmd_install(const char *project_dir, int argc, char **argv) {
    (void)argc; (void)argv;
    Manifest m;
    if (!manifest_load(project_dir, &m)) {
        ui_err("no %s here -- run 'apm init' first", MANIFEST_FILE);
        return 1;
    }

    char ash_modules[1024];
    snprintf(ash_modules, sizeof(ash_modules), "%s/@ash-modules", project_dir);
    fsutil_mkdir_p(ash_modules);

    if (m.deps.count + m.dev_deps.count == 0) {
        ui_info("%s has no dependencies", m.name);
        return 0;
    }

    ui_header("%s@%s", m.name, m.version);
    int ok = install_all(project_dir, &m.deps);
    if (!install_all(project_dir, &m.dev_deps)) ok = 0;
    if (ok) ui_ok("done");
    else ui_err("one or more dependencies failed to install");
    return ok ? 0 : 1;
}

static void list_dependencies(const char *project_dir, const char *title, EntryList *list) {
    if (list->count == 0) return;
    ui_header("%s", title);
    for (int i = 0; i < list->count; i++) {
        char dest[1024];
        snprintf(dest, sizeof(dest), "%s/@ash-modules/%s", project_dir, list->items[i].name);
        if (fsutil_dir_exists(dest)) ui_bullet("%s: %s", list->items[i].name, list->items[i].value);
        else ui_bullet("%s: %s (not installed)", list->items[i].name, list->items[i].value);
    }
}

static void list_entries(const char *title, EntryList *list) {
    if (list->count == 0) return;
    ui_header("%s", title);
    for (int i = 0; i < list->count; i++) ui_bullet("%s: %s", list->items[i].name, list->items[i].value);
}

int cmd_list(const char *project_dir, int argc, char **argv) {
    (void)argc; (void)argv;
    Manifest m;
    if (!manifest_load(project_dir, &m)) {
        ui_err("no %s here -- run 'apm init' first", MANIFEST_FILE);
        return 1;
    }

    ui_header("%s@%s", m.name, m.version);
    if (m.description[0]) ui_bullet("%s", m.description);
    if (m.author[0]) ui_bullet("author: %s", m.author);
    if (m.license[0]) ui_bullet("license: %s", m.license);
    if (m.main[0]) ui_bullet("main: %s", m.main);
    if (m.deps.count + m.dev_deps.count + m.libs.count + m.scripts.count == 0) {
        ui_bullet("(no dependencies)");
        return 0;
    }
    list_dependencies(project_dir, "dependencies", &m.deps);
    list_dependencies(project_dir, "dev-dependencies", &m.dev_deps);
    list_entries("libraries", &m.libs);
    list_entries("scripts", &m.scripts);
    return 0;
}

// apm run <script>   runs [scripts] <script> of ash.pkg with `sh -c`, from the project directory.
// `apm build` and `apm test` are shorthands for `apm run build` / `apm run test`.
// Only the project's own ash.pkg is consulted; scripts of installed dependencies never run.
int cmd_run(const char *project_dir, int argc, char **argv) {
    Manifest m;
    if (!manifest_load(project_dir, &m)) {
        ui_err("no %s here -- run 'apm init' first", MANIFEST_FILE);
        return 1;
    }
    if (argc < 1) {
        ui_err("usage: apm run <script>");
        list_entries("scripts", &m.scripts);
        return 1;
    }
    Entry *script = entry_find(&m.scripts, argv[0]);
    if (!script) {
        ui_err("no script '%s' in the [scripts] section of %s", argv[0], MANIFEST_FILE);
        return 1;
    }
    ui_info("%s: %s", script->name, script->value);
    char cmd[MANIFEST_VALUE_LEN + 1100];
    snprintf(cmd, sizeof(cmd), "cd '%s' && %s", project_dir, script->value);
    int status = system(cmd);
    if (status != 0) {
        ui_err("script '%s' failed", script->name);
        return 1;
    }
    return 0;
}

static void print_license(const License *l) { ui_bullet("%-13s %s", l->id, l->name); }

// apm license                                  lists the available licenses
// apm license <id> [--author <name>] [--year <yyyy>]
//   writes LICENSE (replacing any existing one) and sets `license:` in ash.pkg to <id>, so running it
//   again with another id swaps both. The copyright holder defaults to ash.pkg's `author:` (without
//   the <email>), the year to the current year.
int cmd_license(const char *project_dir, int argc, char **argv) {
    if (argc < 1) {
        ui_header("available licenses");
        license_each(print_license);
        return 0;
    }
    const License *lic = license_find(argv[0]);
    if (!lic) {
        ui_err("unknown license '%s' -- run 'apm license' to list the available ones", argv[0]);
        return 1;
    }
    const char *author_arg = NULL, *year_arg = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--author") == 0 && i + 1 < argc) author_arg = argv[++i];
        else if (strcmp(argv[i], "--year") == 0 && i + 1 < argc) year_arg = argv[++i];
        else {
            ui_err("usage: apm license <id> [--author <name>] [--year <yyyy>]");
            return 1;
        }
    }

    Manifest m;
    if (!manifest_load(project_dir, &m)) {
        char project_name[MANIFEST_NAME_LEN];
        infer_project_name(project_name, sizeof(project_name));
        manifest_init_default(&m, project_name);
        ui_info("no %s here, creating one", MANIFEST_FILE);
    }

    char author[MANIFEST_NAME_LEN];
    if (author_arg) snprintf(author, sizeof(author), "%s", author_arg);
    else {
        snprintf(author, sizeof(author), "%s", m.author);
        char *angle = strchr(author, '<');
        if (angle) { *angle = '\0'; size_t n = strlen(author); while (n > 0 && author[n - 1] == ' ') author[--n] = '\0'; }
    }
    if (license_needs_author(lic) && author[0] == '\0') {
        ui_err("%s needs a copyright holder: pass --author <name> or set 'author:' in %s", lic->id, MANIFEST_FILE);
        return 1;
    }

    char year[16];
    if (year_arg) snprintf(year, sizeof(year), "%s", year_arg);
    else {
        time_t now = time(NULL);
        strftime(year, sizeof(year), "%Y", localtime(&now));
    }

    char *text = license_render(lic, year, author);
    if (!text) { ui_err("out of memory"); return 1; }
    char path[1024];
    snprintf(path, sizeof(path), "%s/LICENSE", project_dir);
    int existed = access(path, F_OK) == 0;
    FILE *f = fopen(path, "w");
    if (!f) { ui_err("could not write %s", path); free(text); return 1; }
    fputs(text, f);
    fclose(f);
    free(text);

    snprintf(m.license, sizeof(m.license), "%s", lic->id);
    if (!manifest_save(project_dir, &m)) return 1;
    ui_ok("%s LICENSE (%s) and set license: %s in %s", existed ? "replaced" : "wrote", lic->name, lic->id, MANIFEST_FILE);
    return 0;
}
