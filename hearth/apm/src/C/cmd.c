#define _XOPEN_SOURCE 700 // exposes PATH_MAX under -std=c11 (see ashvm/kiln's import_paths.c)
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <stdarg.h>
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

// Package id for a module name: lowercase, every non-alphanumeric run becomes '_' ("My-Lib" -> "my_lib").
static void package_id(const char *name, char *out, int out_size) {
    int n = 0;
    for (const char *c = name; *c && n < out_size - 2; c++) {
        char ch = *c;
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        int word = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9');
        if (word) out[n++] = ch;
        else if (n > 0 && out[n - 1] != '_') out[n++] = '_';
    }
    while (n > 0 && out[n - 1] == '_') n--;
    if (n == 0 || (out[0] >= '0' && out[0] <= '9')) { memmove(out + 1, out, (size_t)n); out[0] = '_'; n++; }
    out[n] = '\0';
}

// Class name for a package id: "my_lib" -> "MyLib".
static void class_id(const char *pkg, char *out, int out_size) {
    int n = 0, up = 1;
    for (const char *c = pkg; *c && n < out_size - 1; c++) {
        if (*c == '_') { up = 1; continue; }
        out[n++] = (up && *c >= 'a' && *c <= 'z') ? (char)(*c - 'a' + 'A') : *c;
        up = 0;
    }
    out[n] = '\0';
}

static int write_file(const char *path, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static int write_file(const char *path, const char *fmt, ...) {
    FILE *f = fopen(path, "w");
    if (!f) { ui_err("could not write %s", path); return 0; }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
    return 1;
}

// apm new <name>
//   scaffolds ./<name>/ as an Ash module. Every module has the same shape: the source root is
//   src/main/ash, and a type in package p is src/main/ash/p/<Type>.ash, so users write `import p.Type;`.
int cmd_new(const char *project_dir, int argc, char **argv) {
    if (argc != 1 || argv[0][0] == '\0' || strchr(argv[0], '/')) {
        ui_err("usage: apm new <name>");
        return 1;
    }
    const char *name = argv[0];
    char pkg[MANIFEST_NAME_LEN], cls[MANIFEST_NAME_LEN];
    package_id(name, pkg, sizeof(pkg));
    class_id(pkg, cls, sizeof(cls));

    char root[1024], src[1100], path[1400];
    snprintf(root, sizeof(root), "%s/%s", project_dir, name);
    if (fsutil_dir_exists(root)) { ui_err("%s already exists", root); return 1; }
    snprintf(src, sizeof(src), "%s/src/main/ash/%s", root, pkg);
    if (!fsutil_mkdir_p(src)) { ui_err("could not create %s", src); return 1; }

    Manifest m;
    manifest_init_default(&m, name);
    snprintf(m.main, sizeof(m.main), "%s.%s", pkg, cls);
    snprintf(m.description, sizeof(m.description), "TODO: describe %s", name);
    entry_set(&m.scripts, "build", "for f in $(find src/main/ash -name '*.ash'); do ASH_PATH=src/main/ash ashvm check \"$f\" || exit 1; done");
    if (!manifest_save(root, &m)) return 1;

    snprintf(path, sizeof(path), "%s/%s.ash", src, cls);
    if (!write_file(path,
            "package %s;\n\n/// Entry point of %s.\npublic final class %s {\n"
            "    private %s() { }\n\n    public static String name() {\n        return \"%s\";\n    }\n}\n",
            pkg, name, cls, cls, name)) return 1;
    snprintf(path, sizeof(path), "%s/README.md", root);
    if (!write_file(path,
            "# %s\n\n```sh\napm add github:<user>/%s\n```\n\n```ash\nimport %s.%s;\n```\n",
            name, name, pkg, cls)) return 1;
    snprintf(path, sizeof(path), "%s/.gitignore", root);
    if (!write_file(path, "@ash-modules/\n")) return 1;

    ui_ok("created %s (import %s.%s;)", root, pkg, cls);
    return 0;
}

// ---- apm check --------------------------------------------------------------------------------
// Module conventions, so every Ash module looks the same:
//   ash.pkg has name, version, description, license; README.md and LICENSE exist;
//   sources live under src/main/ash; every .ash file is <Type>.ash (UpperCamelCase), declares
//   `package a.b;` matching its directory a/b, and declares a type named like the file.
typedef struct { int problems; int warnings; } Check;

static void check_fail(Check *c, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void check_fail(Check *c, const char *fmt, ...) {
    char msg[1400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    ui_err("%s", msg);
    c->problems++;
}

static int has_suffix(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

static int file_exists(const char *dir, const char *name) {
    char p[1100];
    snprintf(p, sizeof(p), "%s/%s", dir, name);
    return access(p, F_OK) == 0;
}

// first `package x.y;` of the file, as a path "x/y" ("" = none)
static void read_package(const char *path, char *out, int out_size) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "package ", 8) != 0) continue;
        char *semi = strchr(line, ';');
        if (!semi) break;
        *semi = '\0';
        snprintf(out, (size_t)out_size, "%s", line + 8);
        for (char *c = out; *c; c++) if (*c == '.') *c = '/';
        break;
    }
    fclose(f);
}

static int file_declares_type(const char *path, const char *type) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[512], want[160];
    int found = 0;
    while (!found && fgets(line, sizeof(line), f)) {
        const char *kws[] = { "class ", "interface ", "enum ", "record " };
        for (int i = 0; i < 4 && !found; i++) {
            snprintf(want, sizeof(want), "%s%s", kws[i], type);
            const char *at = strstr(line, want);
            char after = at ? at[strlen(want)] : 0;
            if (at && (after == ' ' || after == '<' || after == '{' || after == '(' || after == '\n' || after == '\0')) found = 1;
        }
    }
    fclose(f);
    return found;
}

static void check_tree(Check *c, const char *root, const char *rel) {
    char dir[1100];
    if (rel[0]) snprintf(dir, sizeof(dir), "%s/%s", root, rel); else snprintf(dir, sizeof(dir), "%s", root);
    DIR *d = opendir(dir);
    if (!d) return;
    for (struct dirent *e = readdir(d); e; e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        char sub[1400];
        snprintf(sub, sizeof(sub), "%s/%s", dir, e->d_name);
        char subrel[1400];
        if (rel[0]) snprintf(subrel, sizeof(subrel), "%s/%s", rel, e->d_name); else snprintf(subrel, sizeof(subrel), "%s", e->d_name);
        if (fsutil_dir_exists(sub)) { check_tree(c, root, subrel); continue; }
        if (!has_suffix(e->d_name, ".ash")) continue;

        char type[160];
        snprintf(type, sizeof(type), "%s", e->d_name);
        type[strlen(type) - 4] = '\0';
        if (type[0] < 'A' || type[0] > 'Z') check_fail(c, "%s: file name must be UpperCamelCase (<Type>.ash)", subrel);
        char pkg[512];
        read_package(sub, pkg, sizeof(pkg));
        char expect[1100] = "";
        if (rel[0]) snprintf(expect, sizeof(expect), "%s", rel);
        if (strcmp(pkg, expect) != 0) check_fail(c, "%s: package must match its directory ('%s')", subrel, expect);
        else if (!file_declares_type(sub, type)) check_fail(c, "%s: must declare a type named %s", subrel, type);
    }
    closedir(d);
}

static void lint_report(int problem, int line, const char *msg, void *ctx) {
    (void)line;
    Check *c = ctx;
    if (problem) { ui_err("%s: %s", MANIFEST_FILE, msg); c->problems++; }
    else { ui_warn("%s: %s", MANIFEST_FILE, msg); c->warnings++; }
}

// unknown keys and sections, a memory-safety value the compiler would misread, duplicates
static void lint_manifest(const char *project_dir, Check *c) { manifest_lint(project_dir, lint_report, c); }

// a dependency name becomes a directory, its source is fetched with git: both must be what install accepts
static void check_dependencies(Check *c, const char *section, EntryList *list) {
    for (int i = 0; i < list->count; i++) {
        const Entry *e = &list->items[i];
        if (!fsutil_safe_name(e->name))
            check_fail(c, "%s: [%s] '%s' is not a usable name (letters, digits, '-', '_' and '.' only, not starting with '.')", MANIFEST_FILE, section, e->name);
        else if (!git_fetch_spec_ok(e->value))
            check_fail(c, "%s: [%s] %s: '%s' is not a source apm can fetch (github:<user>/<repo>[@ref])", MANIFEST_FILE, section, e->name, e->value);
    }
}

int cmd_check(const char *project_dir, int argc, char **argv) {
    (void)argc; (void)argv;
    Check c = { 0 };
    Manifest m;
    if (!manifest_load(project_dir, &m)) {
        ui_err("no %s here -- run 'apm init' or 'apm new' first", MANIFEST_FILE);
        return 1;
    }
    if (!m.name[0]) check_fail(&c, "%s: missing name", MANIFEST_FILE);
    if (!m.version[0]) check_fail(&c, "%s: missing version", MANIFEST_FILE);
    if (!m.description[0]) check_fail(&c, "%s: missing description", MANIFEST_FILE);
    if (!m.license[0]) check_fail(&c, "%s: missing license (apm license <id>)", MANIFEST_FILE);
    if (m.main[0]) {
        // main is a qualified type name: pkg.Class is src/main/ash/pkg/Class.ash
        char rel[2 * MANIFEST_NAME_LEN + 32];
        int n = snprintf(rel, sizeof(rel), "src/main/ash/");
        for (const char *q = m.main; *q && n < (int)sizeof(rel) - 5; q++) rel[n++] = *q == '.' ? '/' : *q;
        snprintf(rel + n, sizeof(rel) - (size_t)n, ".ash");
        if (!file_exists(project_dir, rel)) check_fail(&c, "%s: main '%s' is not a file here (expected %s)", MANIFEST_FILE, m.main, rel);
    }
    lint_manifest(project_dir, &c);
    check_dependencies(&c, "dependencies", &m.deps);
    check_dependencies(&c, "dev-dependencies", &m.dev_deps);
    if (!file_exists(project_dir, "README.md")) check_fail(&c, "missing README.md");
    if (!file_exists(project_dir, "LICENSE")) check_fail(&c, "missing LICENSE (apm license <id>)");

    char root[1100];
    snprintf(root, sizeof(root), "%s/src/main/ash", project_dir);
    if (!fsutil_dir_exists(root)) check_fail(&c, "missing source root src/main/ash");
    else check_tree(&c, root, "");

    if (c.problems) {
        ui_err("%d problem%s%s", c.problems, c.problems == 1 ? "" : "s", c.warnings ? " (and warnings above)" : "");
        return 1;
    }
    if (c.warnings) ui_ok("%s follows the module conventions (%d warning%s)", m.name, c.warnings, c.warnings == 1 ? "" : "s");
    else ui_ok("%s follows the module conventions", m.name);
    return 0;
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
    if (!git_fetch_spec_ok(source)) {
        ui_err("'%s' is not a valid source: expected github:<user>/<repo> or github:<user>/<repo>@<ref> (letters, digits, '-', '_' and '.' only)", source);
        return 1;
    }
    const char *explicit_name = NULL;
    int dev = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--as") == 0 && i + 1 < argc) explicit_name = argv[++i];
        else if (strcmp(argv[i], "--dev") == 0) dev = 1;
        else {
            ui_err("unknown or incomplete option '%s'\nusage: apm add github:<user>/<repo>[@ref] [--as <name>] [--dev]", argv[i]);
            return 1;
        }
    }

    char name[MANIFEST_NAME_LEN];
    if (explicit_name) {
        snprintf(name, sizeof(name), "%s", explicit_name);
    } else {
        git_fetch_infer_name(source, name, sizeof(name));
    }
    if (!fsutil_safe_name(name)) {
        ui_err("'%s' is not a usable dependency name (letters, digits, '-', '_' and '.' only, not starting with '.'); pick one with --as", name);
        return 1;
    }

    Manifest m;
    int had_manifest = manifest_load(project_dir, &m);
    if (!had_manifest) {
        char project_name[MANIFEST_NAME_LEN];
        infer_project_name(project_name, sizeof(project_name));
        manifest_init_default(&m, project_name);
        ui_info("no %s here, creating one", MANIFEST_FILE);
    }
    EntryList *target = dev ? &m.dev_deps : &m.deps;

    char ash_modules[1024], dest[1200];
    snprintf(ash_modules, sizeof(ash_modules), "%s/@ash-modules", project_dir);
    snprintf(dest, sizeof(dest), "%s/%s", ash_modules, name);
    fsutil_mkdir_p(ash_modules);

    // the same name from another source: what is installed came from the old one
    Entry *old = entry_find(target, name);
    if (old && strcmp(old->value, source) != 0 && fsutil_dir_exists(dest)) {
        ui_info("%s was %s, replacing it with %s", name, old->value, source);
        if (!fsutil_rm_rf(dest)) { ui_err("could not remove %s", dest); return 1; }
    }

    // installed first, recorded after: a spec that cannot be fetched must not end up in ash.pkg
    Dependency dep;
    snprintf(dep.name, sizeof(dep.name), "%s", name);
    snprintf(dep.value, sizeof(dep.value), "%s", source);
    if (!install_dependency(project_dir, &dep)) {
        ui_err("failed to install '%s' (%s is unchanged)", name, MANIFEST_FILE);
        return 1;
    }
    if (!entry_set(target, name, source)) {
        ui_err("dependency table full (max %d)", MANIFEST_MAX_ENTRIES);
        return 1;
    }
    if (!manifest_save(project_dir, &m)) return 1;
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
