#define _POSIX_C_SOURCE 200809L // getline under -std=c11
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "H/lock.h"
#include "H/git_fetch.h"
#include "H/fsutil.h"

LockEntry *lock_find(Lock *lock, const char *name) {
    for (int i = 0; i < lock->count; i++) if (strcmp(lock->items[i].name, name) == 0) return &lock->items[i];
    return NULL;
}

int lock_set(Lock *lock, const char *name, const char *spec, const char *sha, const char *via) {
    LockEntry *e = lock_find(lock, name);
    if (!e) {
        if (lock->count >= LOCK_MAX_ENTRIES) return 0;
        e = &lock->items[lock->count++];
    }
    snprintf(e->name, sizeof(e->name), "%s", name);
    snprintf(e->spec, sizeof(e->spec), "%s", spec);
    snprintf(e->sha, sizeof(e->sha), "%s", sha);
    snprintf(e->via, sizeof(e->via), "%s", via ? via : "");
    return 1;
}

void lock_remove(Lock *lock, const char *name) {
    for (int i = 0; i < lock->count; i++) {
        if (strcmp(lock->items[i].name, name) != 0) continue;
        for (int j = i + 1; j < lock->count; j++) lock->items[j - 1] = lock->items[j];
        lock->count--;
        return;
    }
}

char *lock_render(const Lock *lock) {
    size_t cap = 512 + (size_t)lock->count * (MANIFEST_NAME_LEN + MANIFEST_VALUE_LEN + 200);
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t n = (size_t)snprintf(out, cap,
        "# ash.lock -- written by apm; commit it.\n"
        "# `apm install` checks out exactly these commits, whatever a tag or branch points to now;\n"
        "# `apm update [name]` is what moves them. Format of a line: name: source commit [via requiring-package]\n"
        "lock: 1\n\n[packages]\n");
    for (int i = 0; i < lock->count; i++) {
        const LockEntry *e = &lock->items[i];
        n += (size_t)snprintf(out + n, cap - n, "%s: %s %s", e->name, e->spec, e->sha);
        if (e->via[0]) n += (size_t)snprintf(out + n, cap - n, " via %s", e->via);
        n += (size_t)snprintf(out + n, cap - n, "\n");
    }
    return out;
}

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)len, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

int lock_matches_file(const char *dir, const Lock *lock) {
    char path[1100];
    snprintf(path, sizeof(path), "%s/%s", dir, LOCK_FILE);
    char *have = read_all(path);
    char *want = lock_render(lock);
    int same = have && want && strcmp(have, want) == 0;
    free(have);
    free(want);
    return same;
}

int lock_save(const char *dir, const Lock *lock) {
    char path[1100];
    snprintf(path, sizeof(path), "%s/%s", dir, LOCK_FILE);
    if (lock->count == 0) {
        if (access(path, F_OK) == 0) return unlink(path) == 0;
        return 1;
    }
    char *text = lock_render(lock);
    if (!text) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) { free(text); fprintf(stderr, "apm: could not write %s\n", path); return 0; }
    int ok = fputs(text, f) >= 0;
    ok = fclose(f) == 0 && ok;
    free(text);
    return ok;
}

// Splits "a b c" in place; returns the number of words (at most max).
static int words(char *line, char **out, int max) {
    int n = 0;
    char *p = line;
    while (*p && n < max) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        out[n++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return n;
}

int lock_load(const char *dir, Lock *out) {
    char path[1100];
    snprintf(path, sizeof(path), "%s/%s", dir, LOCK_FILE);
    memset(out, 0, sizeof(*out));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char *line = NULL;
    size_t cap = 0;
    ssize_t got;
    int lineno = 0, in_packages = 0, bad = 0;
    while ((got = getline(&line, &cap, f)) >= 0) {
        lineno++;
        while (got > 0 && (line[got - 1] == '\n' || line[got - 1] == '\r' || line[got - 1] == ' ')) line[--got] = '\0';
        char *l = line;
        while (*l == ' ' || *l == '\t') l++;
        if (*l == '\0' || *l == '#') continue;
        if (*l == '[') { in_packages = strcmp(l, "[packages]") == 0; continue; }
        if (!in_packages) {
            if (strncmp(l, "lock:", 5) == 0 && atoi(l + 5) != 1) {
                fprintf(stderr, "apm: %s is a lockfile of version %s, this apm reads version 1; ignoring it\n", LOCK_FILE, l + 5);
                bad = 1;
                break;
            }
            continue;
        }
        char *colon = strchr(l, ':');
        char *w[6];
        if (!colon) { bad = lineno; break; }
        *colon = '\0';
        char *name = l;
        size_t nl = strlen(name);
        while (nl > 0 && name[nl - 1] == ' ') name[--nl] = '\0';
        int n = words(colon + 1, w, 6);
        // spec sha [via parent]
        if (n != 2 && n != 4) { bad = lineno; break; }
        if (n == 4 && strcmp(w[2], "via") != 0) { bad = lineno; break; }
        if (!fsutil_safe_name(name) || !git_fetch_spec_ok(w[0]) || !git_fetch_is_commit(w[1]) || (n == 4 && !fsutil_safe_name(w[3])) ||
            strlen(w[0]) >= MANIFEST_VALUE_LEN) { bad = lineno; break; }
        if (!lock_set(out, name, w[0], w[1], n == 4 ? w[3] : NULL)) { bad = lineno; break; }
    }
    free(line);
    fclose(f);
    if (bad) {
        if (bad > 1 || lineno > 0) fprintf(stderr, "apm: %s line %d is not understood, ignoring %s (the next install writes a new one)\n", LOCK_FILE, lineno, LOCK_FILE);
        memset(out, 0, sizeof(*out));
        return 0;
    }
    return 1;
}
