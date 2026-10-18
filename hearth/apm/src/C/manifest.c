#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "H/manifest.h"

// header keys in file order: where each one lives in Manifest
static const struct { const char *key; size_t offset; size_t size; } header_keys[] = {
    { "name", offsetof(Manifest, name), sizeof(((Manifest *)0)->name) },
    { "version", offsetof(Manifest, version), sizeof(((Manifest *)0)->version) },
    { "description", offsetof(Manifest, description), sizeof(((Manifest *)0)->description) },
    { "author", offsetof(Manifest, author), sizeof(((Manifest *)0)->author) },
    { "license", offsetof(Manifest, license), sizeof(((Manifest *)0)->license) },
    { "homepage", offsetof(Manifest, homepage), sizeof(((Manifest *)0)->homepage) },
    { "repository", offsetof(Manifest, repository), sizeof(((Manifest *)0)->repository) },
    { "bugs", offsetof(Manifest, bugs), sizeof(((Manifest *)0)->bugs) },
    { "main", offsetof(Manifest, main), sizeof(((Manifest *)0)->main) },
    { "keywords", offsetof(Manifest, keywords), sizeof(((Manifest *)0)->keywords) },
    { "memory-safety", offsetof(Manifest, memory_safety), sizeof(((Manifest *)0)->memory_safety) },
};
#define HEADER_KEYS ((int)(sizeof(header_keys) / sizeof(header_keys[0])))

static const struct { const char *title; size_t offset; } sections[] = {
    { "dependencies", offsetof(Manifest, deps) },
    { "dev-dependencies", offsetof(Manifest, dev_deps) },
    { "libraries", offsetof(Manifest, libs) },
    { "scripts", offsetof(Manifest, scripts) },
};
#define SECTIONS ((int)(sizeof(sections) / sizeof(sections[0])))

void manifest_init_default(Manifest *m, const char *project_name) {
    memset(m, 0, sizeof(*m));
    snprintf(m->name, sizeof(m->name), "%s", project_name);
    snprintf(m->version, sizeof(m->version), "0.1.0");
    snprintf(m->memory_safety, sizeof(m->memory_safety), "on");
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
    EntryList *section = NULL; // NULL while in the header
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *l = trim(line);
        if (l[0] == '\0' || l[0] == '#') continue;
        if (l[0] == '[') {
            section = NULL;
            for (int i = 0; i < SECTIONS; i++) {
                char title[64];
                snprintf(title, sizeof(title), "[%s]", sections[i].title);
                if (strcmp(l, title) == 0) section = (EntryList *)((char *)out + sections[i].offset);
            }
            if (!section) fprintf(stderr, "apm: unknown section '%s' in %s, ignoring it\n", l, MANIFEST_FILE);
            continue;
        }

        char *colon = strchr(l, ':');
        if (!colon) continue; // ignore malformed lines rather than aborting the whole load
        *colon = '\0';
        char *key = trim(l);
        char *value = trim(colon + 1);

        if (section) {
            if (!entry_set(section, key, value))
                fprintf(stderr, "apm: too many entries (max %d), ignoring '%s'\n", MANIFEST_MAX_ENTRIES, key);
            continue;
        }
        for (int i = 0; i < HEADER_KEYS; i++) {
            if (strcmp(key, header_keys[i].key) == 0) snprintf((char *)out + header_keys[i].offset, header_keys[i].size, "%s", value);
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
        fprintf(stderr, "apm: could not write %s\n", path);
        return 0;
    }

    for (int i = 0; i < HEADER_KEYS; i++) {
        const char *value = (const char *)m + header_keys[i].offset;
        if (value[0]) fprintf(f, "%s: %s\n", header_keys[i].key, value);
    }
    for (int i = 0; i < SECTIONS; i++) {
        const EntryList *list = (const EntryList *)((const char *)m + sections[i].offset);
        if (list->count == 0) continue;
        fprintf(f, "\n[%s]\n", sections[i].title);
        for (int j = 0; j < list->count; j++) fprintf(f, "%s: %s\n", list->items[j].name, list->items[j].value);
    }
    fclose(f);
    return 1;
}

int entry_set(EntryList *list, const char *name, const char *value) {
    Entry *existing = entry_find(list, name);
    if (existing) {
        snprintf(existing->value, sizeof(existing->value), "%s", value);
        return 1;
    }
    if (list->count >= MANIFEST_MAX_ENTRIES) return 0;
    Entry *e = &list->items[list->count++];
    snprintf(e->name, sizeof(e->name), "%s", name);
    snprintf(e->value, sizeof(e->value), "%s", value);
    return 1;
}

Entry *entry_find(EntryList *list, const char *name) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].name, name) == 0) return &list->items[i];
    }
    return NULL;
}
