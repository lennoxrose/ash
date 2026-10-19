#define _POSIX_C_SOURCE 200809L // getline under -std=c11
#include <limits.h>
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

// What one line of ash.pkg is.
enum { L_OTHER, L_SECTION, L_HEADER, L_ENTRY };
typedef struct {
    int kind;
    int sec;            // L_SECTION / L_ENTRY: index in sections[], -1 for a section apm does not know
    int key;            // L_HEADER: index in header_keys[], -1 for a key apm does not know
    char name[MANIFEST_NAME_LEN];
    char value[MANIFEST_VALUE_LEN];
    int too_long;       // the value did not fit its field
} Parsed;

static int section_index(const char *trimmed_line) {
    for (int i = 0; i < SECTIONS; i++) {
        char title[64];
        snprintf(title, sizeof(title), "[%s]", sections[i].title);
        if (strcmp(trimmed_line, title) == 0) return i;
    }
    return -1;
}

// Classifies a line given the section the previous lines left us in (-2 = still in the header).
static void parse_line(const char *raw, int *cur_sec, Parsed *p) {
    char buf[MANIFEST_NAME_LEN + MANIFEST_VALUE_LEN + 16];
    memset(p, 0, sizeof(*p));
    p->sec = -1;
    p->key = -1;
    // lines longer than the buffer are cut; the value then reports too_long
    snprintf(buf, sizeof(buf), "%s", raw);
    char *l = trim(buf);
    if (l[0] == '\0' || l[0] == '#') return;
    if (l[0] == '[') {
        p->kind = L_SECTION;
        p->sec = section_index(l);
        *cur_sec = p->sec >= 0 ? p->sec : -1;
        return;
    }
    char *colon = strchr(l, ':');
    if (!colon) return; // ignore malformed lines rather than aborting the whole load
    *colon = '\0';
    char *key = trim(l);
    char *value = trim(colon + 1);
    snprintf(p->name, sizeof(p->name), "%s", key);
    snprintf(p->value, sizeof(p->value), "%s", value);
    p->too_long = strlen(raw) >= sizeof(buf) - 1 || strlen(key) >= sizeof(p->name) || strlen(value) >= sizeof(p->value);
    if (*cur_sec == -2) {
        p->kind = L_HEADER;
        for (int i = 0; i < HEADER_KEYS; i++) if (strcmp(key, header_keys[i].key) == 0) p->key = i;
    } else if (*cur_sec >= 0) {
        p->kind = L_ENTRY;
        p->sec = *cur_sec;
    }                                                    // an entry of an unknown section stays L_OTHER
}

// All lines of the file (without the line ending); NULL-terminated array in *out, count returned, -1 if the
// file cannot be read.
static int read_lines(const char *path, char ***out) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char **v = NULL;
    int n = 0;
    size_t cap = 0;
    char *line = NULL;
    size_t sz = 0;
    ssize_t got;
    while ((got = getline(&line, &sz, f)) >= 0) {
        while (got > 0 && (line[got - 1] == '\n' || line[got - 1] == '\r')) line[--got] = '\0';
        if (n == INT_MAX - 1) break;                      // more lines than a manifest can have: the rest is ignored
        if ((size_t)n == cap) {
            size_t ncap = cap ? cap * 2 : 64;
            char **nv = realloc(v, ncap * sizeof(char *));
            if (!nv) break;                               // out of memory: work with the lines read so far
            v = nv;
            cap = ncap;
        }
        char *copy = strdup(line);
        if (!copy) break;
        v[n++] = copy;
    }
    free(line);
    fclose(f);
    *out = v;
    return n;
}

static void free_lines(char **v, int n) {
    for (int i = 0; i < n; i++) free(v[i]);
    free(v);
}

int manifest_load(const char *dir, Manifest *out) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, MANIFEST_FILE);
    char **lines;
    int n = read_lines(path, &lines);
    if (n < 0) return 0;

    memset(out, 0, sizeof(*out));
    int cur = -2;
    for (int i = 0; i < n; i++) {
        Parsed p;
        parse_line(lines[i], &cur, &p);
        if (p.kind == L_SECTION && p.sec < 0) {
            fprintf(stderr, "apm: unknown section '%s' in %s (line %d), ignoring it\n", lines[i], MANIFEST_FILE, i + 1);
            continue;
        }
        if (p.too_long) fprintf(stderr, "apm: %s line %d is too long (a value holds at most %d characters), it is cut\n", MANIFEST_FILE, i + 1, MANIFEST_VALUE_LEN - 1);
        if (p.kind == L_ENTRY) {
            EntryList *section = (EntryList *)((char *)out + sections[p.sec].offset);
            if (!entry_set(section, p.name, p.value))
                fprintf(stderr, "apm: too many entries (max %d), ignoring '%s'\n", MANIFEST_MAX_ENTRIES, p.name);
        } else if (p.kind == L_HEADER && p.key >= 0) {
            snprintf((char *)out + header_keys[p.key].offset, header_keys[p.key].size, "%s", p.value);
        }
    }
    free_lines(lines, n);
    return 1;
}

static void put_entry_line(FILE *f, const char *name, const char *value) { fprintf(f, "%s: %s\n", name, value); }

static void write_fresh(FILE *f, const Manifest *m) {
    for (int i = 0; i < HEADER_KEYS; i++) {
        const char *value = (const char *)m + header_keys[i].offset;
        if (value[0]) fprintf(f, "%s: %s\n", header_keys[i].key, value);
    }
    for (int i = 0; i < SECTIONS; i++) {
        const EntryList *list = (const EntryList *)((const char *)m + sections[i].offset);
        if (list->count == 0) continue;
        fprintf(f, "\n[%s]\n", sections[i].title);
        for (int j = 0; j < list->count; j++) put_entry_line(f, list->items[j].name, list->items[j].value);
    }
}

static int blank_or_comment(const char *raw) {
    while (*raw == ' ' || *raw == '\t') raw++;
    return *raw == '\0' || *raw == '#';
}

// Saving over an existing ash.pkg edits it instead of regenerating it: comments, blank lines, keys and sections
// apm does not know and the order the author chose all stay; only lines whose value changed are rewritten,
// removed ones dropped, and new keys / entries added (header keys at the end of the header, entries at the end of
// their section, a missing section at the end of the file).
static int save_patched(FILE *f, const Manifest *m, char **lines, int n) {
    Parsed *ps = calloc((size_t)n + 1, sizeof(Parsed));
    if (!ps) return 0;
    int cur = -2;
    int seen_hdr[16] = {0};
    int seen_ent[SECTIONS][MANIFEST_MAX_ENTRIES] = {{0}};
    int first_section = n;
    int last_block_start[SECTIONS], last_block_end[SECTIONS];
    for (int i = 0; i < SECTIONS; i++) last_block_start[i] = last_block_end[i] = -1;
    for (int i = 0; i < n; i++) {
        parse_line(lines[i], &cur, &ps[i]);
        if (ps[i].kind == L_SECTION) {
            if (first_section == n) first_section = i;
            if (ps[i].sec >= 0) last_block_start[ps[i].sec] = i;
        }
    }
    // a section's block ends at the next section line
    for (int s = 0; s < SECTIONS; s++) {
        if (last_block_start[s] < 0) continue;
        int e = last_block_start[s] + 1;
        while (e < n && ps[e].kind != L_SECTION) e++;
        last_block_end[s] = e;
    }
    for (int i = 0; i < n; i++) {
        if (ps[i].kind == L_HEADER && ps[i].key >= 0) seen_hdr[ps[i].key] = 1;
        if (ps[i].kind == L_ENTRY) {
            const EntryList *list = (const EntryList *)((const char *)m + sections[ps[i].sec].offset);
            for (int j = 0; j < list->count; j++) if (strcmp(list->items[j].name, ps[i].name) == 0) seen_ent[ps[i].sec][j] = 1;
        }
    }
    // where pending lines go: before the blank / comment lines that precede the next block
    int hdr_at = first_section;
    while (hdr_at > 0 && blank_or_comment(lines[hdr_at - 1])) hdr_at--;
    int flush_at[SECTIONS];
    for (int s = 0; s < SECTIONS; s++) {
        flush_at[s] = -1;
        if (last_block_start[s] < 0) continue;
        int at = last_block_end[s];
        while (at > last_block_start[s] + 1 && blank_or_comment(lines[at - 1])) at--;
        flush_at[s] = at;
    }
    int hdr_done[16] = {0};
    int ent_done[SECTIONS][MANIFEST_MAX_ENTRIES] = {{0}};
    for (int i = 0; i <= n; i++) {
        if (i == hdr_at) {
            for (int k = 0; k < HEADER_KEYS; k++) {
                const char *value = (const char *)m + header_keys[k].offset;
                if (!seen_hdr[k] && value[0]) fprintf(f, "%s: %s\n", header_keys[k].key, value);
            }
        }
        for (int s = 0; s < SECTIONS; s++) {
            if (i != flush_at[s]) continue;
            const EntryList *list = (const EntryList *)((const char *)m + sections[s].offset);
            for (int j = 0; j < list->count; j++) if (!seen_ent[s][j]) put_entry_line(f, list->items[j].name, list->items[j].value);
        }
        if (i == n) break;
        Parsed *p = &ps[i];
        if (p->kind == L_HEADER && p->key >= 0) {
            const char *value = (const char *)m + header_keys[p->key].offset;
            if (hdr_done[p->key] || !value[0]) continue;     // a repeated key, or one that was emptied
            hdr_done[p->key] = 1;
            if (strcmp(value, p->value) == 0) fprintf(f, "%s\n", lines[i]);
            else fprintf(f, "%s: %s\n", header_keys[p->key].key, value);
        } else if (p->kind == L_ENTRY) {
            const EntryList *list = (const EntryList *)((const char *)m + sections[p->sec].offset);
            int j = 0;
            while (j < list->count && strcmp(list->items[j].name, p->name) != 0) j++;
            if (j == list->count || ent_done[p->sec][j]) continue;
            ent_done[p->sec][j] = 1;
            if (strcmp(list->items[j].value, p->value) == 0) fprintf(f, "%s\n", lines[i]);
            else put_entry_line(f, list->items[j].name, list->items[j].value);
        } else {
            fprintf(f, "%s\n", lines[i]);
        }
    }
    // sections the file did not have
    for (int s = 0; s < SECTIONS; s++) {
        const EntryList *list = (const EntryList *)((const char *)m + sections[s].offset);
        if (last_block_start[s] >= 0 || list->count == 0) continue;
        fprintf(f, "\n[%s]\n", sections[s].title);
        for (int j = 0; j < list->count; j++) put_entry_line(f, list->items[j].name, list->items[j].value);
    }
    free(ps);
    return 1;
}

int manifest_save(const char *dir, const Manifest *m) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, MANIFEST_FILE);
    char **old = NULL;
    int n = read_lines(path, &old);
    FILE *f = fopen(path, "w");                          // (the old text is already in memory)
    if (!f) {
        fprintf(stderr, "apm: could not write %s\n", path);
        if (n >= 0) free_lines(old, n);
        return 0;
    }
    if (n >= 0) {
        save_patched(f, m, old, n);
        free_lines(old, n);
    } else {
        write_fresh(f, m);
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

// ---- lint -------------------------------------------------------------------------------------

static int edit_distance(const char *a, const char *b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la > 40 || lb > 40) return 99;
    int d[41][41];
    for (int i = 0; i <= la; i++) d[i][0] = i;
    for (int j = 0; j <= lb; j++) d[0][j] = j;
    for (int i = 1; i <= la; i++) for (int j = 1; j <= lb; j++) {
        int c = d[i - 1][j - 1] + (a[i - 1] != b[j - 1]);
        if (d[i - 1][j] + 1 < c) c = d[i - 1][j] + 1;
        if (d[i][j - 1] + 1 < c) c = d[i][j - 1] + 1;
        d[i][j] = c;
    }
    return d[la][lb];
}

// the known key that `key` was most likely meant to be ("memory_safety" -> "memory-safety"), or NULL
static const char *closest_key(const char *key) {
    char norm[MANIFEST_NAME_LEN];
    size_t k = 0;
    for (; key[k] && k < sizeof(norm) - 1; k++) norm[k] = key[k] == '_' ? '-' : (key[k] >= 'A' && key[k] <= 'Z' ? (char)(key[k] + 32) : key[k]);
    norm[k] = '\0';
    const char *best = NULL;
    int best_d = 3;
    for (int i = 0; i < HEADER_KEYS; i++) {
        int d = edit_distance(norm, header_keys[i].key);
        if (d < best_d) { best_d = d; best = header_keys[i].key; }
    }
    return best;
}

static const char *closest_section(const char *title) {
    const char *best = NULL;
    int best_d = 3;
    for (int i = 0; i < SECTIONS; i++) {
        int d = edit_distance(title, sections[i].title);
        if (d < best_d) { best_d = d; best = sections[i].title; }
    }
    return best;
}

void manifest_lint(const char *dir, ManifestLintFn report, void *ctx) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, MANIFEST_FILE);
    char **lines;
    int n = read_lines(path, &lines);
    if (n < 0) return;
    char msg[600];
    int cur = -2;
    int seen_hdr[16] = {0};
    int seen_section[SECTIONS] = {0};
    char entry_seen[SECTIONS][MANIFEST_MAX_ENTRIES][MANIFEST_NAME_LEN];
    int entry_count[SECTIONS] = {0};
    for (int i = 0; i < n; i++) {
        Parsed p;
        int line = i + 1;
        parse_line(lines[i], &cur, &p);
        char *t = lines[i];
        while (*t == ' ' || *t == '\t') t++;
        if (p.kind == L_OTHER && !blank_or_comment(lines[i]) && t[0] != '[' && !strchr(t, ':')) {
            snprintf(msg, sizeof(msg), "line %d is not a 'key: value' line, so it is ignored: %.60s", line, t);
            report(0, line, msg, ctx);
        } else if (p.kind == L_SECTION) {
            if (p.sec < 0) {
                char title[80];
                snprintf(title, sizeof(title), "%s", t + 1);
                char *close = strchr(title, ']');
                if (close) *close = '\0';
                const char *guess = closest_section(title);
                snprintf(msg, sizeof(msg), "unknown section '%.60s' (line %d) is ignored%s%s%s", t, line, guess ? "; did you mean [" : "", guess ? guess : "", guess ? "]?" : "");
                report(0, line, msg, ctx);
            } else if (seen_section[p.sec]++) {
                snprintf(msg, sizeof(msg), "section [%s] appears twice (line %d); keep its entries in one place", sections[p.sec].title, line);
                report(0, line, msg, ctx);
            }
        } else if (p.kind == L_HEADER) {
            if (p.key < 0) {
                const char *guess = closest_key(p.name);
                snprintf(msg, sizeof(msg), "unknown key '%.60s' (line %d) is ignored%s%s%s", p.name, line, guess ? "; did you mean '" : "", guess ? guess : "", guess ? "'?" : "");
                report(0, line, msg, ctx);
            } else {
                if (seen_hdr[p.key]++) {
                    snprintf(msg, sizeof(msg), "key '%s' is set twice (line %d); the last one wins", p.name, line);
                    report(0, line, msg, ctx);
                }
                if (strcmp(p.name, "memory-safety") == 0) {
                    if (strcmp(p.value, "on") != 0 && strcmp(p.value, "off") != 0) {
                        snprintf(msg, sizeof(msg), "memory-safety must be 'on' or 'off', not '%.40s' (line %d): the compiler treats everything that does not start with 'off' as 'on'", p.value, line);
                        report(1, line, msg, ctx);
                    } else if (strcmp(p.value, "off") == 0) {
                        snprintf(msg, sizeof(msg), "memory-safety is off (line %d): no ownership, borrow, range or initialisation checks, and no automatic freeing; set it to 'on' unless this project needs System.free", line);
                        report(0, line, msg, ctx);
                    }
                }
            }
        } else if (p.kind == L_ENTRY) {
            if (strcmp(p.name, "memory-safety") == 0) {
                snprintf(msg, sizeof(msg), "'memory-safety' is under [%s] (line %d), where it is just an entry: the compiler only reads it in the header, before the first [section]", sections[p.sec].title, line);
                report(0, line, msg, ctx);
            }
            int dup = 0;
            for (int j = 0; j < entry_count[p.sec]; j++) if (strcmp(entry_seen[p.sec][j], p.name) == 0) dup = 1;
            if (dup) {
                snprintf(msg, sizeof(msg), "'%.60s' is listed twice in [%s] (line %d); the last one wins", p.name, sections[p.sec].title, line);
                report(0, line, msg, ctx);
            } else if (entry_count[p.sec] < MANIFEST_MAX_ENTRIES) {
                snprintf(entry_seen[p.sec][entry_count[p.sec]++], MANIFEST_NAME_LEN, "%s", p.name);
            }
        }
        if (p.too_long) {
            snprintf(msg, sizeof(msg), "line %d is longer than apm reads (a value holds at most %d characters)", line, MANIFEST_VALUE_LEN - 1);
            report(1, line, msg, ctx);
        }
    }
    free_lines(lines, n);
}
