#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "H/hub/hubcmd.h"
#include "H/hub/client.h"
#include "H/hub/spec.h"
#include "H/manifest.h"
#include "H/ui.h"

// "owner/repo" from the argument, else from ash.pkg's `repository: github:owner/repo[@ref]`.
static int target(const char *project_dir, int argc, char **argv, HubSpec *out) {
    if (argc >= 1) {
        if (hubspec_parse(argv[0], out) && out->version[0] == '\0') return 1;
        ui_err("expected owner/repo, got '%s'", argv[0]);
        return 0;
    }
    Manifest *m = malloc(sizeof(*m));
    int ok = 0;
    if (m && manifest_load(project_dir, m) && strncmp(m->repository, "github:", 7) == 0) {
        char spec[MANIFEST_VALUE_LEN];
        snprintf(spec, sizeof(spec), "%s", m->repository + 7);
        char *at = strchr(spec, '@');
        if (at) *at = '\0';
        ok = hubspec_parse(spec, out);
    }
    free(m);
    if (!ok) ui_err("give owner/repo, or set 'repository: github:owner/repo' in %s", MANIFEST_FILE);
    return ok;
}

static void print_package(const Json *p) {
    const char *state = json_gets(p, "status"), *valid = json_gets(p, "validity");
    ui_bullet("%s/%s  v%s%s%s%s", json_gets(p, "owner"), json_gets(p, "repo"), json_gets(p, "version") ? json_gets(p, "version") : "?",
              state ? "  " : "", state ? state : "", (valid && strcmp(valid, "VALID") != 0) ? " (ash.pkg not valid)" : "");
}

// One call with the saved token, on the package of `target`; `verb` is what to say on success.
static int package_call(const char *project_dir, int argc, char **argv, const char *method, const char *suffix, const char *verb) {
    HubSpec t;
    if (!target(project_dir, argc, argv, &t)) return 1;
    Auth a;
    config_load(&a);
    char path[400], err[512];
    snprintf(path, sizeof(path), "/api/v1/me/packages/%s/%s%s", t.owner, t.repo, suffix);
    Json *res = NULL;
    int status = 0;
    if (!hub_call(&a, method, path, 1, NULL, &status, &res, err, sizeof(err))) {
        ui_err("%s/%s: %s", t.owner, t.repo, err);
        json_free(res);
        return 1;
    }
    ui_ok("%s/%s %s", t.owner, t.repo, (status == 200 && strcmp(method, "PUT") == 0) ? "is already linked on the hub" : verb);
    const Json *p = json_get(res, "package");
    if (p) print_package(p);
    if (p && strcmp(method, "DELETE") != 0) ui_bullet("%s/packages/%s/%s", a.hub, t.owner, t.repo);
    json_free(res);
    return 0;
}

int cmd_link(const char *project_dir, int argc, char **argv) { return package_call(project_dir, argc, argv, "PUT", "", "linked on the hub"); }
int cmd_sync(const char *project_dir, int argc, char **argv) { return package_call(project_dir, argc, argv, "POST", "/sync", "synced"); }
int cmd_unlink(const char *project_dir, int argc, char **argv) { return package_call(project_dir, argc, argv, "DELETE", "", "unlinked from the hub (the repository is untouched)"); }

int cmd_packages(const char *project_dir, int argc, char **argv) {
    (void)project_dir; (void)argc; (void)argv;
    Auth a;
    config_load(&a);
    char err[512];
    Json *res = NULL;
    if (!hub_call(&a, "GET", "/api/v1/me/packages", 1, NULL, NULL, &res, err, sizeof(err))) { ui_err("%s", err); json_free(res); return 1; }
    const Json *list = json_get(res, "packages");
    if (!list || list->count == 0) ui_info("you have not linked any packages yet (apm link owner/repo)");
    for (size_t i = 0; list && i < list->count; i++) print_package(json_at(list, i));
    json_free(res);
    return 0;
}

int cmd_search(const char *project_dir, int argc, char **argv) {
    (void)project_dir;
    char query[200] = "", enc[640], path[760], err[512];
    for (int i = 0; i < argc; i++) {
        if (strlen(query) + strlen(argv[i]) + 2 >= sizeof(query)) break;
        if (i) strcat(query, " ");
        strcat(query, argv[i]);
    }
    if (!hub_urlencode(query, enc, sizeof(enc))) { ui_err("search words too long"); return 1; }
    snprintf(path, sizeof(path), "/api/v1/search?q=%s&limit=20", enc);
    Auth a;
    config_load(&a);
    Json *res = NULL;
    if (!hub_call(&a, "GET", path, 0, NULL, NULL, &res, err, sizeof(err))) { ui_err("%s", err); json_free(res); return 1; }
    const Json *items = json_get(res, "items"), *total = json_get(res, "total");
    if (!items || items->count == 0) ui_info("no packages found");
    for (size_t i = 0; items && i < items->count; i++) {
        const Json *p = json_at(items, i);
        ui_bullet("%s/%s  v%s  %s", json_gets(p, "owner"), json_gets(p, "repo"), json_gets(p, "version"), json_gets(p, "description") ? json_gets(p, "description") : "");
    }
    if (total && total->type == J_NUM && items && total->num > (double)items->count) ui_info("%d more on %s/search?q=%s", (int)(total->num - (double)items->count), a.hub, enc);
    json_free(res);
    return 0;
}

int cmd_info(const char *project_dir, int argc, char **argv) {
    (void)project_dir;
    HubSpec t;
    if (argc != 1 || !hubspec_parse(argv[0], &t) || t.version[0]) { ui_err("usage: apm info <owner/repo>"); return 1; }
    char path[400], err[512];
    snprintf(path, sizeof(path), "/api/v1/packages/%s/%s", t.owner, t.repo);
    Auth a;
    config_load(&a);
    Json *res = NULL;
    int status = 0;
    if (!hub_call(&a, "GET", path, 0, NULL, &status, &res, err, sizeof(err))) { ui_err("%s", status == 404 ? "no such package on the hub" : err); json_free(res); return 1; }
    const Json *p = json_get(res, "package"), *w = json_get(p, "warnings");
    ui_header("%s/%s  v%s", json_gets(p, "owner"), json_gets(p, "repo"), json_gets(p, "version"));
    if (json_gets(p, "description")) ui_bullet("%s", json_gets(p, "description"));
    if (json_gets(p, "license")) ui_bullet("license: %s", json_gets(p, "license"));
    ui_bullet("source: %s", json_gets(p, "github"));
    ui_bullet("latest tag: %s", json_gets(p, "latestVersion") ? json_gets(p, "latestVersion") : "(none, tracks the default branch)");
    for (size_t i = 0; w && i < w->count; i++) ui_bullet("warning: %s", json_str(json_at(w, i)));
    ui_info("install with: apm add %s/%s", t.owner, t.repo);
    json_free(res);
    return 0;
}
