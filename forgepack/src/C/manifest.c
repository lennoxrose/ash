#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "H/manifest.h"

void manifest_init_default(Manifest *m, const char *project_name) {
    memset(m, 0, sizeof(*m));
    snprintf(m->name, sizeof(m->name), "%s", project_name);
    snprintf(m->version, sizeof(m->version), "0.1.0");
}

// Trims leading/trailing whitespace in place, returns the trimmed start.
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
    return s;
}

int manifest_load(const char *dir, Manifest *out) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, MANIFEST_FILE);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    memset(out, 0, sizeof(*out));
    int in_deps = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *l = trim(line);
        if (l[0] == '\0' || l[0] == '#') continue;
        if (strcmp(l, "[dependencies]") == 0) { in_deps = 1; continue; }

        char *colon = strchr(l, ':');
        if (!colon) continue; // ignore malformed lines rather than aborting the whole load
        *colon = '\0';
        char *key = trim(l);
        char *value = trim(colon + 1);

        if (!in_deps) {
            if (strcmp(key, "name") == 0) snprintf(out->name, sizeof(out->name), "%s", value);
            else if (strcmp(key, "version") == 0) snprintf(out->version, sizeof(out->version), "%s", value);
        } else {
            if (out->dep_count >= MANIFEST_MAX_DEPS) {
                fprintf(stderr, "forgepack: too many dependencies (max %d), ignoring '%s'\n", MANIFEST_MAX_DEPS, key);
                continue;
            }
            Dependency *d = &out->deps[out->dep_count++];
            snprintf(d->name, sizeof(d->name), "%s", key);
            snprintf(d->value, sizeof(d->value), "%s", value);
        }
    }
    fclose(f);
    return 1;
}

int manifest_save(const char *dir, const Manifest *m) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, MANIFEST_FILE);
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "forgepack: could not write %s\n", path);
        return 0;
    }

    fprintf(f, "name: %s\n", m->name);
    fprintf(f, "version: %s\n", m->version);
    if (m->dep_count > 0) {
        fprintf(f, "\n[dependencies]\n");
        for (int i = 0; i < m->dep_count; i++) {
            fprintf(f, "%s: %s\n", m->deps[i].name, m->deps[i].value);
        }
    }
    fclose(f);
    return 1;
}

int manifest_set_dependency(Manifest *m, const char *name, const char *value) {
    Dependency *existing = manifest_find_dependency(m, name);
    if (existing) {
        snprintf(existing->value, sizeof(existing->value), "%s", value);
        return 1;
    }
    if (m->dep_count >= MANIFEST_MAX_DEPS) return 0;
    Dependency *d = &m->deps[m->dep_count++];
    snprintf(d->name, sizeof(d->name), "%s", name);
    snprintf(d->value, sizeof(d->value), "%s", value);
    return 1;
}

Dependency *manifest_find_dependency(Manifest *m, const char *name) {
    for (int i = 0; i < m->dep_count; i++) {
        if (strcmp(m->deps[i].name, name) == 0) return &m->deps[i];
    }
    return NULL;
}
