#include <string.h>

#include "jlower/H/jlower_internal.h"

/* Dead-code elimination over the generated module. The old VM addresses functions with one byte
 * (256 per program) and allows 256 module variables, and the lowering emits whole classes plus a
 * runtime prelude, so everything unreachable from `start` is dropped:
 *   - a function is kept when its name is called/mentioned in kept text,
 *   - a vtable entry `C__VT["key"] = fn;` is kept when the string "key" appears in kept text
 *     (every dynamic call site and runtime helper spells its key as a literal),
 *   - a module variable is kept when it is mentioned in kept text. */

typedef struct {
    char *text;
    char *name; /* forge or local name; NULL for comments */
    int is_fn;
    int keep;
} Part;

typedef struct VtLine {
    char *line;  /* the whole VT assignment line */
    char *key;   /* "toString/0" without quotes */
    char *fn;
    int keep;
    struct VtLine *next; /* another line with the same key */
} VtLine;

static int ident_char(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

static unsigned name_hash(const char *s, size_t n) {
    unsigned h = 2166136261u;
    for (size_t i = 0; i < n; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}


typedef struct {
    Part **pslot;       /* parts by name (open addressing) */
    VtLine **vslot;     /* VT lines by key, chained through next */
    int cap;
    const char **work;  /* kept texts not read yet */
    int nwork;
} Reach;

/* every identifier in [p, end) that names a part keeps that part */
static void reach_words(Reach *r, const char *p, const char *end) {
    while (p < end) {
        if (!ident_char(*p)) { p++; continue; }
        const char *e = p;
        while (e < end && ident_char(*e)) e++;
        size_t n = (size_t)(e - p);
        unsigned h = name_hash(p, n) & (unsigned)(r->cap - 1);
        while (r->pslot[h]) {
            Part *c = r->pslot[h];
            if (strlen(c->name) == n && !strncmp(c->name, p, n)) {
                if (!c->keep) { c->keep = 1; r->work[r->nwork++] = c->text; }
                break;
            }
            h = (h + 1) & (unsigned)(r->cap - 1);
        }
        p = e;
    }
}

/* Reads a kept text once: a word that names a part keeps it (words inside string literals count, as they always
 * did), a string literal that spells the key of a VT line keeps that line. */
static void reach_text(Reach *r, const char *t) {
    for (const char *p = t; *p;) {
        if (*p != '"') {
            const char *e = p;
            while (*e && *e != '"') e++;
            reach_words(r, p, e);
            p = e;
            continue;
        }
        const char *e = p + 1;
        while (*e && *e != '"') e += (*e == '\\' && e[1]) ? 2 : 1;
        size_t n = (size_t)(e - (p + 1));
        unsigned h = name_hash(p + 1, n) & (unsigned)(r->cap - 1);
        while (r->vslot[h]) {
            if (strlen(r->vslot[h]->key) == n && !strncmp(r->vslot[h]->key, p + 1, n)) {
                for (VtLine *v = r->vslot[h]; v; v = v->next) {
                    if (!v->keep) { v->keep = 1; r->work[r->nwork++] = v->line; }
                }
                break;
            }
            h = (h + 1) & (unsigned)(r->cap - 1);
        }
        reach_words(r, p + 1, e);
        p = *e ? e + 1 : e;
    }
}

static char *take_name(const char *s, const char *prefix) {
    s += strlen(prefix);
    const char *e = s;
    while (ident_char(*e)) e++;
    return j_strndup(s, (size_t)(e - s));
}

char *prune_module(const char *module) {
    JList parts = {0}, vts = {0};
    JBuf start_pre = {0}, start_post = {0};
    int start_idx = -1;
    const char *p = module;
    while (*p) {
        const char *end = p;
        do {
            const char *nl = strchr(end, '\n');
            end = nl ? nl + 1 : end + strlen(end);
        } while (*end && strncmp(end, "forge ", 6) != 0 && strncmp(end, "local ", 6) != 0 && strncmp(end, "// ----", 7) != 0);
        Part *c = j_alloc(sizeof(Part));
        c->text = j_strndup(p, (size_t)(end - p));
        if (!strncmp(p, "forge ", 6)) { c->name = take_name(p, "forge "); c->is_fn = 1; }
        else if (!strncmp(p, "local ", 6)) c->name = take_name(p, "local ");
        if (c->is_fn && !strcmp(c->name, "start")) start_idx = parts.count;
        j_list_push(&parts, c);
        p = end;
    }
    if (start_idx < 0) return (char *)module;
    /* split start() into VT assignment lines (conditional) and everything else (always kept) */
    Part *st = parts.items[start_idx];
    JBuf fixed = {0};
    for (char *line = st->text; *line;) {
        char *nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        char *l = j_strndup(line, len);
        char *vt = strstr(l, "__VT[\"");
        if (vt && strstr(l, "] = ") && l[len - 1] == ';') {
            VtLine *v = j_alloc(sizeof(VtLine));
            v->line = l;
            char *ks = vt + 6;
            char *ke = strstr(ks, "\"] = ");
            v->key = j_strndup(ks, (size_t)(ke - ks));
            char *fs = ke + 5;
            v->fn = j_strndup(fs, strlen(fs) - 1);
            j_list_push(&vts, v);
        } else {
            j_buf_add(&fixed, l);
            j_buf_addc(&fixed, '\n');
        }
        if (!nl) break;
        line = nl + 1;
    }
    st->text = fixed.data ? fixed.data : (char *)"";
    st->keep = 1;
    /* Reachability from the always-kept text (every part without a name, and start() without its VT lines).
     * Each text is read once: a word that names a part keeps it, a string literal that spells the key of a VT line
     * keeps that line; a kept part or line is then read in turn. */
    int cap = 64;
    while (cap < 4 * (parts.count + vts.count) + 16) cap *= 2;
    Part **pslot = j_alloc(sizeof(Part *) * (size_t)cap);       /* part by name */
    VtLine **vslot = j_alloc(sizeof(VtLine *) * (size_t)cap);   /* VT lines by key, chained through next */
    for (int i = 0; i < parts.count; i++) {
        Part *c = parts.items[i];
        if (!c->name) continue;
        unsigned h = name_hash(c->name, strlen(c->name)) & (unsigned)(cap - 1);
        while (pslot[h]) h = (h + 1) & (unsigned)(cap - 1);
        pslot[h] = c;
    }
    for (int i = 0; i < vts.count; i++) {
        VtLine *v = vts.items[i];
        unsigned h = name_hash(v->key, strlen(v->key)) & (unsigned)(cap - 1);
        while (vslot[h] && strcmp(vslot[h]->key, v->key) != 0) h = (h + 1) & (unsigned)(cap - 1);
        v->next = vslot[h];
        vslot[h] = v;
    }
    Reach r = {pslot, vslot, cap, j_alloc(sizeof(char *) * (size_t)(parts.count + vts.count + 2)), 0};
    for (int i = 0; i < parts.count; i++) {
        Part *c = parts.items[i];
        if (!c->name || c->keep) r.work[r.nwork++] = c->text;
    }
    while (r.nwork > 0) reach_text(&r, r.work[--r.nwork]);
    JBuf out = {0};
    for (int i = 0; i < parts.count; i++) {
        Part *c = parts.items[i];
        if (i == start_idx) {
            /* re-insert the kept VT lines right after `forge start() {` */
            char *nl = strchr(c->text, '\n');
            size_t head = nl ? (size_t)(nl - c->text) + 1 : strlen(c->text);
            j_buf_addn(&out, c->text, head);
            for (int k = 0; k < vts.count; k++) {
                VtLine *v = vts.items[k];
                if (v->keep) { j_buf_add(&out, v->line); j_buf_addc(&out, '\n'); }
            }
            j_buf_add(&out, c->text + head);
        } else if (!c->name || c->keep) {
            j_buf_add(&out, c->text);
        }
    }
    (void)start_pre;
    (void)start_post;
    return out.data ? out.data : (char *)"";
}
