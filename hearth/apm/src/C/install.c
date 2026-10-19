#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "H/install.h"
#include "H/fsutil.h"
#include "H/git_fetch.h"
#include "H/lock.h"
#include "H/ui.h"

// GitHub-only for now, deliberately -- no registry/index yet (see
// ideas/ash_modules.md's phased plan), so a git source is the only thing
// apm can actually resolve without a server. path:/plain-version
// dependencies are a later addition, not scoped in yet.

typedef struct {
    char name[MANIFEST_NAME_LEN];
    char spec[MANIFEST_VALUE_LEN];
    char via[MANIFEST_NAME_LEN];       // the package whose ash.pkg asked for it; empty: the project's own
} Want;

typedef struct {
    const char *dir;
    Want *queue;                       // every package to install, in the order they were first asked for
    int count;
    Lock *old;                         // ash.lock as it was
    Lock *now;                         // what this run resolved
    char (*unlocked)[MANIFEST_NAME_LEN]; // packages to look up again whatever ash.lock says
    int unlocked_count;
    int update_all;
    int locked;
} Run;

static int is_unlocked(const Run *r, const char *name) {
    if (r->update_all) return 1;
    for (int i = 0; i < r->unlocked_count; i++) if (strcmp(r->unlocked[i], name) == 0) return 1;
    return 0;
}

// The package, and every package that only the ones in the set brought in (ash.lock remembers who required whom).
static void unlock_with_dependents(Run *r, const char *name) {
    int added = 1;
    snprintf(r->unlocked[r->unlocked_count++], MANIFEST_NAME_LEN, "%s", name);
    while (added) {
        added = 0;
        for (int i = 0; i < r->old->count; i++) {
            const LockEntry *e = &r->old->items[i];
            if (!e->via[0] || is_unlocked(r, e->name) || !is_unlocked(r, e->via)) continue;
            if (r->unlocked_count >= LOCK_MAX_ENTRIES) return;
            snprintf(r->unlocked[r->unlocked_count++], MANIFEST_NAME_LEN, "%s", e->name);
            added = 1;
        }
    }
}

// Adds a package to the queue unless a package of that name is there already (then only a different source is
// news: the first one asked for stays).
static int want(Run *r, const char *name, const char *spec, const char *via) {
    // the name becomes a directory under @ash-modules: "../x" or a name with a space or ';' must never get that far
    if (!fsutil_safe_name(name)) {
        ui_err("'%s'%s%s is not a usable dependency name (letters, digits, '-', '_' and '.' only, not starting with '.')", name, via[0] ? " required by " : "", via);
        return 0;
    }
    if (!git_fetch_is_github_spec(spec)) {
        ui_err("'%s' is not a github: dependency ('%s')%s%s -- only github:user/repo[@ref] is supported right now", name, spec, via[0] ? ", required by " : "", via);
        return 0;
    }
    if (!git_fetch_spec_ok(spec)) {
        ui_err("'%s' is not a valid source for '%s'%s%s: expected github:<user>/<repo> or github:<user>/<repo>@<ref> (letters, digits, '-', '_' and '.' only)", spec, name, via[0] ? ", required by " : "", via);
        return 0;
    }
    for (int i = 0; i < r->count; i++) {
        if (strcmp(r->queue[i].name, name) != 0) continue;
        if (strcmp(r->queue[i].spec, spec) != 0)
            ui_warn("%s wants %s from %s, but %s is already required from %s by %s: keeping that one", via[0] ? via : MANIFEST_FILE, name, spec,
                    name, r->queue[i].spec, r->queue[i].via[0] ? r->queue[i].via : MANIFEST_FILE);
        return 1;
    }
    if (r->count >= LOCK_MAX_ENTRIES) {
        ui_err("too many packages (max %d)", LOCK_MAX_ENTRIES);
        return 0;
    }
    Want *w = &r->queue[r->count++];
    snprintf(w->name, sizeof(w->name), "%s", name);
    snprintf(w->spec, sizeof(w->spec), "%s", spec);
    snprintf(w->via, sizeof(w->via), "%s", via);
    return 1;
}

static void short_sha(const char *sha, char *out) { snprintf(out, 13, "%.12s", sha); }

// Puts one package where it belongs and fills `sha` with the commit it is at. Returns 0 on failure (already told).
static int place(Run *r, const Want *w, char *sha, int sha_size) {
    char dest[1024];
    snprintf(dest, sizeof(dest), "%s/@ash-modules/%s", r->dir, w->name);
    const LockEntry *le = is_unlocked(r, w->name) ? NULL : lock_find(r->old, w->name);
    const char *pinned = le && strcmp(le->spec, w->spec) == 0 ? le->sha : NULL;
    int other_source = le && !pinned;                // ash.lock has the name, from another source: what is installed is that one
    int fetched_fresh = 0;
    char have[48] = "";
    int installed = fsutil_dir_exists(dest);
    if (installed) git_fetch_head(dest, have, sizeof(have));

    if (installed && pinned && strcmp(have, pinned) == 0) {
        snprintf(sha, (size_t)sha_size, "%s", pinned);
        ui_bullet("%s already installed", w->name);
    } else if (installed && !pinned && !other_source && !is_unlocked(r, w->name)) {
        // nothing says otherwise (no lockfile, or a package it never saw): keep it and record where it is
        if (!have[0]) {
            sha[0] = '\0';
            ui_warn("%s is not a git checkout, so apm cannot tell which commit it is at; it stays out of %s", w->name, LOCK_FILE);
        } else {
            snprintf(sha, (size_t)sha_size, "%s", have);
            ui_bullet("%s already installed", w->name);
        }
    } else {
        if (r->locked) {
            if (!pinned) { ui_err("%s is not pinned in %s for %s: run 'apm install' and commit the new lockfile", w->name, LOCK_FILE, w->spec); return 0; }
        }
        if (installed) {
            char a[13], b[13];
            short_sha(have[0] ? have : "none", a);
            if (pinned) { short_sha(pinned, b); ui_info("restoring %s to %s (it was at %s)", w->name, b, a); }
            else ui_info("looking %s up again", w->name);
            if (!fsutil_rm_rf(dest)) { ui_err("could not remove %s", dest); return 0; }
        }
        if (pinned) ui_info("fetching %s (%s, commit %.12s)", w->name, w->spec, pinned);
        else ui_info("fetching %s (%s)", w->name, w->spec);
        if (!git_fetch_github(w->spec, dest, pinned, sha, sha_size)) return 0;
        fetched_fresh = 1;
    }
    char root[1100];
    snprintf(root, sizeof(root), "%s/src/main/ash", dest);
    if (!fsutil_dir_exists(root)) {
        ui_err("'%s' has no src/main/ash source root -- every Ash module must keep its sources there (apm new scaffolds one)", w->name);
        if (!fsutil_rm_rf(dest)) ui_err("could not remove %s", dest);
        return 0;
    }
    if (fetched_fresh) {
        char s[13];
        short_sha(sha, s);
        ui_ok("%s installed (%s)", w->name, s);
    }
    return 1;
}

int install_resolve(const char *project_dir, const Manifest *m, const InstallOpts *opts) {
    Run r = {0};
    r.dir = project_dir;
    r.queue = calloc(LOCK_MAX_ENTRIES, sizeof(Want));
    r.old = calloc(1, sizeof(Lock));
    r.now = calloc(1, sizeof(Lock));
    r.unlocked = calloc(LOCK_MAX_ENTRIES, MANIFEST_NAME_LEN);
    Manifest *dm = calloc(1, sizeof(Manifest));
    if (!r.queue || !r.old || !r.now || !r.unlocked || !dm) { ui_err("out of memory"); return 0; }
    int ok = 1, had_lock = lock_load(project_dir, r.old);
    r.update_all = opts->update_all;
    r.locked = opts->locked;
    if (opts->update_name && opts->update_name[0]) unlock_with_dependents(&r, opts->update_name);
    if (r.locked && !had_lock) {
        ui_err("--locked needs an %s (run 'apm install' first)", LOCK_FILE);
        ok = 0;
    }

    const EntryList *roots[2] = { &m->deps, &m->dev_deps };
    for (int k = 0; ok && k < 2; k++)
        for (int i = 0; i < roots[k]->count; i++) {
            if (!want(&r, roots[k]->items[i].name, roots[k]->items[i].value, "")) ok = 0;
        }

    for (int i = 0; ok && i < r.count; i++) {
        Want w = r.queue[i];
        char sha[48] = "";
        if (!place(&r, &w, sha, sizeof(sha))) { ok = 0; break; }
        if (sha[0]) lock_set(r.now, w.name, w.spec, sha, w.via);
        // its own dependencies (not its dev-dependencies) come along
        char dest[1024];
        snprintf(dest, sizeof(dest), "%s/@ash-modules/%s", project_dir, w.name);
        if (manifest_load(dest, dm)) {
            for (int j = 0; j < dm->deps.count; j++)
                if (!want(&r, dm->deps.items[j].name, dm->deps.items[j].value, w.name)) ok = 0;
        }
    }

    if (ok) {
        // packages the lockfile had installed that nothing needs now
        for (int i = 0; i < r.old->count; i++) {
            const char *name = r.old->items[i].name;
            if (lock_find(r.now, name)) continue;
            char dest[1024];
            snprintf(dest, sizeof(dest), "%s/@ash-modules/%s", project_dir, name);
            if (r.locked) { ui_err("%s is in %s but nothing needs it any more: run 'apm install'", name, LOCK_FILE); ok = 0; continue; }
            if (!fsutil_dir_exists(dest)) continue;
            if (fsutil_rm_rf(dest)) ui_info("removed %s (nothing needs it any more)", name);
            else { ui_err("could not remove %s", dest); ok = 0; }
        }
    }
    if (ok && r.locked && !lock_matches_file(project_dir, r.now)) {
        ui_err("%s does not match %s: run 'apm install' and commit the new lockfile", LOCK_FILE, MANIFEST_FILE);
        ok = 0;
    }
    if (ok && !r.locked) {
        for (int i = 0; i < r.now->count; i++) {
            const LockEntry *n = &r.now->items[i];
            const LockEntry *o = lock_find(r.old, n->name);
            char a[13], b[13];
            short_sha(n->sha, b);
            if (o && strcmp(o->sha, n->sha) != 0) { short_sha(o->sha, a); ui_ok("%s: %s -> %s", n->name, a, b); }
            else if (!o && had_lock) ui_info("%s locked at %s", n->name, b);
            else if (o && is_unlocked(&r, n->name)) ui_bullet("%s is up to date (%s)", n->name, b);
        }
        if ((r.now->count > 0 || had_lock) && !lock_matches_file(project_dir, r.now)) {
            if (!lock_save(project_dir, r.now)) ok = 0;
            else ui_info(r.now->count ? "wrote %s" : "removed %s (no packages)", LOCK_FILE);
        }
    }
    free(dm);
    free(r.queue);
    free(r.old);
    free(r.now);
    free(r.unlocked);
    return ok;
}
