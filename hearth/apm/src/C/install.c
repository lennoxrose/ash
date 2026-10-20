#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "H/install.h"
#include "H/fsutil.h"
#include "H/git_fetch.h"
#include "H/ui.h"
#include "H/hub/client.h"
#include "H/hub/proc.h"
#include "H/hub/spec.h"

#define MAX_DEPTH 16

// "https://github.com/<owner>/<repo>" -> owner, repo. The hub names the repository, apm re-validates what it got.
static int parse_github_url(const char *url, HubSpec *out) {
    const char *prefix = "https://github.com/";
    if (!url || strncmp(url, prefix, strlen(prefix)) != 0) return 0;
    return hubspec_parse(url + strlen(prefix), out) && out->version[0] == '\0';
}

static void note_warnings(const char *name, const Json *res) {
    const Json *w = json_get(res, "warnings");
    for (size_t i = 0; w && i < w->count; i++) {
        const char *code = json_str(json_at(w, i));
        if (code && strcmp(code, "package_invalid") == 0) ui_info("%s: the hub reports its ash.pkg as invalid", name);
        if (code && strcmp(code, "no_tags") == 0) ui_info("%s: no tagged versions, using the head of the default branch", name);
    }
}

static int fetch_from_hub(const Dependency *dep, const char *dest) {
    HubSpec spec, repo;
    if (!hubspec_parse(dep->value, &spec)) {
        ui_err("'%s' is not a valid dependency ('%s'): use owner/repo[@version] or github:user/repo[@ref]", dep->name, dep->value);
        return 0;
    }
    Auth auth;
    config_load(&auth); // only for the hub URL: resolving needs no login
    char path[600], query[220] = "", err[512];
    if (spec.version[0]) { char enc[210]; hub_urlencode(spec.version, enc, sizeof(enc)); snprintf(query, sizeof(query), "?version=%s", enc); }
    snprintf(path, sizeof(path), "/api/v1/resolve/%s/%s%s", spec.owner, spec.repo, query);
    Json *res = NULL;
    int status = 0;
    ui_info("resolving %s on %s", dep->value, auth.hub);
    if (!hub_call(&auth, "GET", path, 0, NULL, &status, &res, err, sizeof(err))) {
        ui_err("%s: %s", dep->name, status == 404 ? "no such package (or version) on the hub" : err);
        json_free(res);
        return 0;
    }
    const Json *ref = json_get(res, "ref");
    const char *commit = json_gets(ref, "commit"), *type = json_gets(ref, "type"), *refname = json_gets(ref, "name");
    if (!parse_github_url(json_gets(res, "github"), &repo)) { ui_err("%s: the hub sent no usable repository", dep->name); json_free(res); return 0; }
    note_warnings(dep->name, res);
    int ok = git_fetch_repo(repo.owner, repo.repo, commit, (!commit && type && strcmp(type, "tag") == 0) ? refname : NULL, dest);
    if (ok) {
        char file[1200];
        snprintf(file, sizeof(file), "%s/.apm-source", dest);
        FILE *f = fopen(file, "w");
        if (f) { fprintf(f, "%s %s\n", dep->value, commit ? commit : "-"); fclose(f); }
    }
    json_free(res);
    return ok;
}

static void remove_tree(const char *dir) {
    char *rm[] = { "rm", "-rf", "--", (char *)dir, NULL }, *out;
    if (proc_run(rm, NULL, 0, &out, 4096, NULL) == 0) free(out);
    else ui_err("could not remove %s", dir);
}

int install_dependency(const char *project_dir, const Dependency *dep) {
    static int depth = 0;
    char dest[1024];
    if (!name_is_safe(dep->name)) {
        ui_err("'%s' is not a valid dependency name (letters, digits, '.', '_' and '-', starting with a letter or digit)", dep->name);
        return 0;
    }
    snprintf(dest, sizeof(dest), "%s/@ash-modules/%s", project_dir, dep->name);

    if (fsutil_dir_exists(dest)) {
        ui_bullet("%s already installed", dep->name);
        return 1;
    }
    if (depth >= MAX_DEPTH) { ui_err("%s: dependency chain deeper than %d levels", dep->name, MAX_DEPTH); return 0; }

    ui_info("fetching %s (%s)", dep->name, dep->value);
    if (!(git_fetch_is_github_spec(dep->value) ? git_fetch_github(dep->value, dest) : fetch_from_hub(dep, dest))) return 0;
    char root[1100];
    snprintf(root, sizeof(root), "%s/src/main/ash", dest);
    if (!fsutil_dir_exists(root)) {
        ui_err("'%s' has no src/main/ash source root -- every Ash module must keep its sources there (apm new scaffolds one)", dep->name);
        remove_tree(dest);
        return 0;
    }
    ui_ok("%s installed", dep->name);

    Manifest *own = malloc(sizeof(*own)); // large: heap, since this recurses
    int ok = 1;
    if (own && manifest_load(dest, own)) {
        depth++;
        for (int i = 0; i < own->deps.count; i++) if (!install_dependency(project_dir, &own->deps.items[i])) ok = 0;
        depth--;
    }
    free(own);
    return ok;
}
