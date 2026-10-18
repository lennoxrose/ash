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

typedef struct {
    char *line;  /* the whole VT assignment line */
    char *key;   /* "toString/0" without quotes */
    char *fn;
    int keep;
} VtLine;

static int ident_char(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

/* does `text` mention identifier `name` as a whole word (call, reference or variable use)? */
static int mentions_word(const char *text, const char *name) {
    size_t n = strlen(name);
    for (const char *p = text; (p = strstr(p, name)); p += n) {
        if ((p == text || !ident_char(p[-1])) && !ident_char(p[n])) return 1;
    }
    return 0;
}

static int mentions_key(const char *text, const char *key) {
    char *q = j_fmt("\"%s\"", key);
    return strstr(text, q) != NULL;
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
    for (int changed = 1; changed;) {
        changed = 0;
        JBuf kept = {0};
        for (int i = 0; i < parts.count; i++) {
            Part *c = parts.items[i];
            if (c->keep || !c->name) j_buf_add(&kept, c->text);
        }
        for (int i = 0; i < vts.count; i++) {
            VtLine *v = vts.items[i];
            if (v->keep) { j_buf_add(&kept, v->line); j_buf_addc(&kept, 10); }
        }
        const char *k = kept.data ? kept.data : "";
        for (int i = 0; i < parts.count; i++) {
            Part *c = parts.items[i];
            if (c->keep || !c->name) continue;
            if (mentions_word(k, c->name)) { c->keep = 1; changed = 1; }
        }
        for (int i = 0; i < vts.count; i++) {
            VtLine *v = vts.items[i];
            if (!v->keep && mentions_key(k, v->key)) { v->keep = 1; changed = 1; }
        }
    }
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
