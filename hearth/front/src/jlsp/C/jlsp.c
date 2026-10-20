#define _POSIX_C_SOURCE 200809L /* fork, poll, setrlimit, alarm under -std=c11 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "jcommon/H/jcommon.h"
#include "jdriver/H/jdriver.h"
#include "jlsp/H/jlsp.h"
#include "jown/H/own.h"
#include "jparse/H/jparse.h"
#include "jtypes/H/check.h"
#include "jtypes/H/jtypes.h"

/* ================================================================== JSON (read)
 * A token array in preorder over the message text: nothing is copied until a string is asked for. Objects and arrays
 * record where their subtree ends, so skipping a value is one assignment. Depth and size are bounded: a message that
 * is not JSON, or nests too deep, is refused, never followed. */

enum { J_OBJECT = 1, J_ARRAY, J_STRING, J_PRIM };

typedef struct {
    int type;
    int start, end;   /* J_STRING: the text between the quotes, undecoded; J_PRIM: the literal; containers: the whole value */
    int size;         /* children (object: key + value pairs count 2 per member) */
    int next;         /* index of the token after this value's subtree */
} Tok;

typedef struct {
    const char *s;
    int len;
    Tok *toks;
    int count, cap;
    int fail;
} JP;

#define JSON_MAX_DEPTH 64
#define JSON_MAX_TOKENS 2000000

static int jp_new(JP *p, int type, int start) {
    if (p->count == p->cap) {
        if (p->cap >= JSON_MAX_TOKENS) { p->fail = 1; return -1; }
        int ncap = p->cap ? p->cap * 2 : 256;
        Tok *nt = realloc(p->toks, (size_t)ncap * sizeof(Tok));
        if (!nt) { p->fail = 1; return -1; }
        p->toks = nt;
        p->cap = ncap;
    }
    Tok *t = &p->toks[p->count];
    t->type = type;
    t->start = start;
    t->end = start;
    t->size = 0;
    t->next = p->count + 1;
    return p->count++;
}

static int jp_ws(JP *p, int i) {
    while (i < p->len && (p->s[i] == ' ' || p->s[i] == '\t' || p->s[i] == '\n' || p->s[i] == '\r')) i++;
    return i;
}

static int jp_value(JP *p, int i, int depth);

static int jp_string(JP *p, int i) {            /* s[i] == '"' */
    int tok = jp_new(p, J_STRING, i + 1);
    if (tok < 0) return -1;
    i++;
    while (i < p->len && p->s[i] != '"') {
        unsigned char c = (unsigned char)p->s[i];
        if (c < 0x20) { p->fail = 1; return -1; }
        if (c == '\\') { i++; if (i >= p->len) { p->fail = 1; return -1; } }
        i++;
    }
    if (i >= p->len) { p->fail = 1; return -1; }
    p->toks[tok].end = i;
    return i + 1;
}

static int jp_value(JP *p, int i, int depth) {
    if (depth > JSON_MAX_DEPTH) { p->fail = 1; return -1; }
    i = jp_ws(p, i);
    if (i >= p->len) { p->fail = 1; return -1; }
    char c = p->s[i];
    if (c == '"') {
        i = jp_string(p, i);
        if (i < 0) return -1;
        return i;
    }
    if (c == '{' || c == '[') {
        int tok = jp_new(p, c == '{' ? J_OBJECT : J_ARRAY, i);
        if (tok < 0) return -1;
        char close = c == '{' ? '}' : ']';
        i = jp_ws(p, i + 1);
        int members = 0;
        if (i < p->len && p->s[i] == close) { p->toks[tok].end = i + 1; p->toks[tok].next = p->count; return i + 1; }
        for (;;) {
            if (c == '{') {
                i = jp_ws(p, i);
                if (i >= p->len || p->s[i] != '"') { p->fail = 1; return -1; }
                i = jp_string(p, i);
                if (i < 0) return -1;
                i = jp_ws(p, i);
                if (i >= p->len || p->s[i] != ':') { p->fail = 1; return -1; }
                i++;
                members++;
            }
            i = jp_value(p, i, depth + 1);
            if (i < 0) return -1;
            members++;
            i = jp_ws(p, i);
            if (i >= p->len) { p->fail = 1; return -1; }
            if (p->s[i] == ',') { i++; continue; }
            if (p->s[i] == close) break;
            p->fail = 1;
            return -1;
        }
        p->toks[tok].size = members;
        p->toks[tok].end = i + 1;
        p->toks[tok].next = p->count;
        return i + 1;
    }
    int start = i;                                  /* number, true, false, null */
    while (i < p->len && (isalnum((unsigned char)p->s[i]) || p->s[i] == '-' || p->s[i] == '+' || p->s[i] == '.')) i++;
    if (i == start) { p->fail = 1; return -1; }
    int tok = jp_new(p, J_PRIM, start);
    if (tok < 0) return -1;
    p->toks[tok].end = i;
    return i;
}

/* 1 when the whole text is one JSON value */
static int jp_parse(JP *p, const char *s, int len) {
    memset(p, 0, sizeof *p);
    p->s = s;
    p->len = len;
    int i = jp_value(p, 0, 0);
    if (i >= 0) i = jp_ws(p, i);
    if (p->fail || i != len || p->count == 0) { free(p->toks); p->toks = NULL; return 0; }
    return 1;
}

/* the value of `key` in the object token `obj`, or -1 */
static int jp_get(const JP *p, int obj, const char *key) {
    if (obj < 0 || p->toks[obj].type != J_OBJECT) return -1;
    size_t kl = strlen(key);
    int i = obj + 1;
    for (int m = 0; m < p->toks[obj].size / 2; m++) {
        const Tok *k = &p->toks[i];
        int v = k->next;
        if ((size_t)(k->end - k->start) == kl && memcmp(p->s + k->start, key, kl) == 0) return v;
        i = p->toks[v].next;
    }
    return -1;
}

static int hex4(const char *s) {
    int v = 0;
    for (int i = 0; i < 4; i++) {
        char c = s[i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return -1;
        v = v * 16 + d;
    }
    return v;
}

static void utf8_put(JBuf *b, unsigned cp) {
    char t[4];
    int n;
    if (cp < 0x80) { t[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { t[0] = (char)(0xC0 | (cp >> 6)); t[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
    else if (cp < 0x10000) { t[0] = (char)(0xE0 | (cp >> 12)); t[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); t[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
    else { t[0] = (char)(0xF0 | (cp >> 18)); t[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); t[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); t[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
    j_buf_addn(b, t, (size_t)n);
}

/* the decoded text of a string token (malloc'd); NULL when the token is not a string */
static char *jp_str(const JP *p, int tok) {
    if (tok < 0 || p->toks[tok].type != J_STRING) return NULL;
    JBuf b = {0};
    int i = p->toks[tok].start, end = p->toks[tok].end;
    while (i < end) {
        char c = p->s[i];
        if (c != '\\') { j_buf_addc(&b, c); i++; continue; }
        i++;
        char e = p->s[i++];
        switch (e) {
            case 'n': j_buf_addc(&b, '\n'); break;
            case 't': j_buf_addc(&b, '\t'); break;
            case 'r': j_buf_addc(&b, '\r'); break;
            case 'b': j_buf_addc(&b, '\b'); break;
            case 'f': j_buf_addc(&b, '\f'); break;
            case 'u': {
                int cp = i + 4 <= end ? hex4(p->s + i) : -1;
                if (cp < 0) { j_buf_addc(&b, '?'); break; }
                i += 4;
                if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= end && p->s[i] == '\\' && p->s[i + 1] == 'u') {
                    int lo = hex4(p->s + i + 2);
                    if (lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); i += 6; }
                }
                utf8_put(&b, (unsigned)cp);
                break;
            }
            default: j_buf_addc(&b, e);            /* \" \\ \/ */
        }
    }
    if (!b.data) { b.data = malloc(1); if (b.data) b.data[0] = 0; }
    return b.data;
}

static long jp_int(const JP *p, int tok, long dflt) {
    if (tok < 0 || p->toks[tok].type != J_PRIM) return dflt;
    char buf[32];
    int n = p->toks[tok].end - p->toks[tok].start;
    if (n <= 0 || n >= (int)sizeof buf) return dflt;
    memcpy(buf, p->s + p->toks[tok].start, (size_t)n);
    buf[n] = 0;
    char *e;
    long v = strtol(buf, &e, 10);
    return *e ? dflt : v;
}

/* ================================================================== JSON (write) */

/* length of the valid UTF-8 sequence at p (1 to 4), or 0 when the bytes there are not one */
static int utf8_valid(const unsigned char *p) {
    if (p[0] < 0x80) return 1;
    if (p[0] >= 0xC2 && p[0] <= 0xDF) return (p[1] & 0xC0) == 0x80 ? 2 : 0;
    if (p[0] >= 0xE0 && p[0] <= 0xEF) {
        if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return 0;
        if (p[0] == 0xE0 && p[1] < 0xA0) return 0;                     /* overlong */
        if (p[0] == 0xED && p[1] >= 0xA0) return 0;                    /* a surrogate */
        return 3;
    }
    if (p[0] >= 0xF0 && p[0] <= 0xF4) {
        if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) return 0;
        if (p[0] == 0xF0 && p[1] < 0x90) return 0;
        if (p[0] == 0xF4 && p[1] >= 0x90) return 0;
        return 4;
    }
    return 0;
}

static void js(JBuf *b, const char *s) {           /* a JSON string literal (text that is not UTF-8 gets U+FFFD: a client must always get valid JSON) */
    j_buf_addc(b, '"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p;) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') { j_buf_addc(b, '\\'); j_buf_addc(b, (char)c); p++; }
        else if (c == '\n') { j_buf_add(b, "\\n"); p++; }
        else if (c == '\r') { j_buf_add(b, "\\r"); p++; }
        else if (c == '\t') { j_buf_add(b, "\\t"); p++; }
        else if (c < 0x20 || c == 0x7F) { j_buf_addf(b, "\\u%04x", c); p++; }
        else if (c < 0x80) { j_buf_addc(b, (char)c); p++; }
        else {
            int n = utf8_valid(p);
            if (n) { j_buf_addn(b, (const char *)p, (size_t)n); p += n; }
            else { j_buf_add(b, "\\ufffd"); p++; }
        }
    }
    j_buf_addc(b, '"');
}

/* ================================================================== text and positions */

/* LSP counts characters in UTF-16 code units (unless the client agreed to UTF-8); the compiler counts bytes from 1. */
static int g_utf8;                                   /* the client takes UTF-8 offsets */

static const char *line_start(const char *text, int line0) {
    const char *p = text;
    while (line0 > 0 && *p) {
        const char *nl = strchr(p, '\n');
        if (!nl) return NULL;
        p = nl + 1;
        line0--;
    }
    return p;
}

static int line_len(const char *ls) {
    const char *e = ls;
    while (*e && *e != '\n') e++;
    if (e > ls && e[-1] == '\r') e--;
    return (int)(e - ls);
}

/* LSP character -> 1-based byte column of the compiler */
static int to_byte_col(const char *text, int line0, int ch) {
    const char *ls = line_start(text, line0);
    if (!ls) return 1;
    int n = line_len(ls), i = 0;
    if (g_utf8) return ch < 0 ? 1 : (ch > n ? n + 1 : ch + 1);
    int units = 0;
    while (i < n && units < ch) {
        unsigned char c = (unsigned char)ls[i];
        int bytes = c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        units += bytes == 4 ? 2 : 1;
        i += bytes;
    }
    return (i > n ? n : i) + 1;
}

/* 1-based byte column -> LSP character */
static int to_lsp_char(const char *text, int line1, int col1) {
    const char *ls = line_start(text, line1 - 1);
    if (!ls || col1 < 1) return 0;
    int n = line_len(ls);
    int upto = col1 - 1 > n ? n : col1 - 1;
    if (g_utf8) return upto;
    int units = 0;
    for (int i = 0; i < upto;) {
        unsigned char c = (unsigned char)ls[i];
        int bytes = c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        units += bytes == 4 ? 2 : 1;
        i += bytes;
    }
    return units;
}

static int is_word(char c) { return isalnum((unsigned char)c) || c == '_' || (unsigned char)c >= 0x80; }

/* length in bytes of the identifier, number or string literal that starts at (line1, col1), or 1 */
static int word_at(const char *text, int line1, int col1) {
    const char *ls = line_start(text, line1 - 1);
    if (!ls || col1 < 1) return 1;
    int n = line_len(ls);
    int i = col1 - 1, k = i;
    if (i < n && (ls[i] == '"' || ls[i] == '\'')) {                 /* up to and including the closing quote */
        for (k = i + 1; k < n && ls[k] != ls[i]; k++) if (ls[k] == '\\') k++;
        return k < n ? k + 1 - i : n - i;
    }
    while (k < n && is_word(ls[k])) k++;
    return k > i ? k - i : 1;
}

/* ================================================================== files and URIs */

static char *uri_to_path(const char *uri) {
    if (strncmp(uri, "file://", 7) != 0) return NULL;
    const char *p = uri + 7;
    if (*p != '/') {                                /* file://host/path: not ours */
        const char *slash = strchr(p, '/');
        if (!slash) return NULL;
        p = slash;
    }
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == '%' && i + 2 < n && isxdigit((unsigned char)p[i + 1]) && isxdigit((unsigned char)p[i + 2])) {
            char h[3] = { p[i + 1], p[i + 2], 0 };
            out[k++] = (char)strtol(h, NULL, 16);
            i += 2;
        } else {
            out[k++] = p[i];
        }
    }
    out[k] = 0;
    return out;
}

static char *path_to_uri(const char *path) {
    JBuf b = {0};
    j_buf_add(&b, "file://");
    for (const unsigned char *p = (const unsigned char *)path; *p; p++) {
        if (isalnum(*p) || strchr("/-_.~", *p)) j_buf_addc(&b, (char)*p);
        else j_buf_addf(&b, "%%%02X", *p);
    }
    return b.data;
}

/* the nearest directory at or above the file with an ash.pkg, else the file's own directory (malloc'd) */
static char *project_dir(const char *path) {
    char *dir = strdup(path);
    if (!dir) return NULL;
    char *slash = strrchr(dir, '/');
    if (!slash) { free(dir); return strdup("."); }
    *slash = 0;
    if (!*dir) { dir[0] = '/'; dir[1] = 0; }
    char *own = strdup(dir);
    for (;;) {
        char probe[PATH_MAX + 16];
        snprintf(probe, sizeof probe, "%s/ash.pkg", dir);
        if (access(probe, F_OK) == 0) { free(own); return dir; }
        char *s = strrchr(dir, '/');
        if (!s || s == dir) break;
        *s = 0;
    }
    free(dir);
    return own;
}

/* ================================================================== analysis in a child process */

#define ANALYSIS_SECONDS 10
#define ANALYSIS_MEMORY ((rlim_t)3 << 30)
#define ANALYSIS_MAX_OUTPUT (64 << 20)

enum { Q_DIAG = 0, Q_HOVER, Q_DEFINITION };

static void json_none(void) { fputs("null\n", stdout); fflush(stdout); }

/* ---- the search through the checked AST for the identifier under the cursor ---- */

typedef struct {
    const char *text;              /* the identifier token's text pointer: a node made from that token has it as its name */
    int line, col, len;            /* the token */
    Node *hit[16];
    JClass *hit_cls[16];
    JMethod *hit_m[16];
    int nhit;
    const char *path;
} Search;

static void add_hit(Search *s, Node *n, JClass *c, JMethod *m) {
    if (s->nhit < 16) { s->hit[s->nhit] = n; s->hit_cls[s->nhit] = c; s->hit_m[s->nhit] = m; s->nhit++; }
}

static void walk(Search *s, Node *n, JClass *c, JMethod *m, int depth) {
    if (!n || depth > 400) return;
    if (n->name == s->text && n->name) add_hit(s, n, c, m);
    else if (n->kind == N_TYPE && n->name && n->line == s->line && n->col <= s->col && s->col < n->col + (int)strlen(n->name)) add_hit(s, n, c, m);
    if (n->kind == N_METHOD || n->kind == N_CTOR) {
        for (int i = 0; c && i < c->methods.count + c->ctors.count; i++) {
            JMethod *cm = i < c->methods.count ? c->methods.items[i] : c->ctors.items[i - c->methods.count];
            if (cm->decl == n) { m = cm; break; }
        }
    }
    walk(s, n->a, c, m, depth + 1);
    walk(s, n->b, c, m, depth + 1);
    walk(s, n->c, c, m, depth + 1);
    JList *ls[] = { &n->l1, &n->l2, &n->l3, &n->l4, &n->l5 };
    for (int k = 0; k < 5; k++)
        for (int i = 0; i < ls[k]->count; i++) walk(s, ls[k]->items[i], c, m, depth + 1);
}

static const char *mods(int flags) {
    static char buf[64];
    buf[0] = 0;
    if (flags & M_PUBLIC) strcat(buf, "public ");
    if (flags & M_PRIVATE) strcat(buf, "private ");
    if (flags & M_PROTECTED) strcat(buf, "protected ");
    if (flags & M_STATIC) strcat(buf, "static ");
    if (flags & M_FINAL) strcat(buf, "final ");
    if (flags & M_ABSTRACT) strcat(buf, "abstract ");
    return buf;
}

static const char *kind_word(NodeKind k) {
    return k == N_INTERFACE ? "interface" : k == N_ENUM ? "enum" : k == N_RECORD ? "record" : "class";
}

static const char *tystr(JType *t) { return t && t->kind != TY_ERROR ? ty_str(t) : "?"; }

static void sig_method(JBuf *b, JMethod *m) {
    j_buf_add(b, mods(m->flags));
    if (!m->is_ctor) { j_buf_add(b, tystr(m->ret)); j_buf_addc(b, ' '); }
    j_buf_add(b, m->is_ctor && m->cls ? m->cls->name : m->name);
    j_buf_addc(b, '(');
    for (int i = 0; i < m->params.count; i++) {
        JParam *p = m->params.items[i];
        if (i) j_buf_add(b, ", ");
        j_buf_addf(b, "%s %s", tystr(p->type), p->name ? p->name : "");
    }
    j_buf_addc(b, ')');
}

typedef struct {
    JBuf hover;                    /* markdown */
    const char *def_path;
    int def_line, def_col;
    const char *def_name;
    int found;
} Answer;

static void a_local(Answer *a, JLocal *l) {
    if (!l) return;
    j_buf_addf(&a->hover, "%s%s %s", l->is_param ? "(parameter) " : "(local) ", tystr(l->type), l->name);
    if (l->decl) { a->def_path = l->decl->path; a->def_line = l->decl->line; a->def_col = l->decl->col; a->def_name = l->name; }
    a->found = 1;
}

static void a_field(Answer *a, JField *f) {
    if (!f) return;
    j_buf_addf(&a->hover, "%s%s %s", mods(f->flags), tystr(f->type), f->name);
    if (f->cls) j_buf_addf(&a->hover, "   // in %s", f->cls->qname);
    if (f->decl) { a->def_path = f->decl->path; a->def_line = f->decl->line; a->def_col = f->decl->col; a->def_name = f->name; }
    a->found = 1;
}

static void a_method(Answer *a, JMethod *m) {
    if (!m) return;
    sig_method(&a->hover, m);
    if (m->cls) j_buf_addf(&a->hover, "   // in %s", m->cls->qname);
    if (m->decl) { a->def_path = m->decl->path; a->def_line = m->decl->line; a->def_col = m->decl->col; a->def_name = m->is_ctor && m->cls ? m->cls->name : m->name; }
    a->found = 1;
}

static void a_class(Answer *a, JClass *c) {
    if (!c) return;
    j_buf_addf(&a->hover, "%s%s %s", mods(c->flags), kind_word(c->kind), c->qname);
    if (c->super && c->super->cls && c->super->cls != jt_object()) j_buf_addf(&a->hover, " extends %s", tystr(c->super));
    if (c->decl) { a->def_path = c->path; a->def_line = c->decl->line; a->def_col = c->decl->col; a->def_name = c->name; }
    a->found = 1;
}

/* what one node made from the token says about it */
static void describe(Answer *a, Node *n, JClass *cls, JMethod *meth) {
    switch (n->kind) {
        case N_LOCAL: case N_PARAM: case N_FOREACH: case N_CATCH: case N_TYPEPATTERN:
            a_local(a, n->sym);
            return;
        case N_NAME:
            if (n->rk == R_LOCAL) a_local(a, n->sym);
            else if (n->rk == R_CLASS) a_class(a, n->sym);
            else if (n->rk == R_FIELD || n->rk == R_STATIC_FIELD || n->rk == R_THIS_FIELD || n->rk == R_ENUM_CONST) a_field(a, n->sym);
            return;
        case N_FIELDACC:
            if (n->rk == R_CLASS) a_class(a, n->sym);
            else if (n->rk == R_ARRAYLEN) { j_buf_add(&a->hover, "int length"); a->found = 1; }
            else if (n->rk == R_FIELD || n->rk == R_STATIC_FIELD || n->rk == R_THIS_FIELD || n->rk == R_ENUM_CONST) a_field(a, n->sym);
            return;
        case N_CALL: case N_METHODREF: a_method(a, n->sym); return;
        case N_NEW: a_method(a, n->sym); return;
        case N_TYPE: {
            JType *t = n->type;
            if (!t && cls) t = jt_resolve_type(n, cls, meth, 0);
            if (t && t->kind == TY_CLASS) a_class(a, t->cls);
            else if (t && t->kind != TY_ERROR) { j_buf_add(&a->hover, tystr(t)); a->found = 1; }
            return;
        }
        case N_FIELD:
            for (int i = 0; cls && i < cls->fields.count; i++) if (((JField *)cls->fields.items[i])->decl == n) a_field(a, cls->fields.items[i]);
            return;
        case N_METHOD: case N_CTOR:
            for (int i = 0; cls && i < cls->methods.count + cls->ctors.count; i++) {
                JMethod *cm = i < cls->methods.count ? cls->methods.items[i] : cls->ctors.items[i - cls->methods.count];
                if (cm->decl == n) a_method(a, cm);
            }
            return;
        case N_CLASS: case N_INTERFACE: case N_ENUM: case N_RECORD:
            if (cls) a_class(a, cls);
            return;
        default:
            if (n->type && n->type->kind != TY_ERROR) { j_buf_addf(&a->hover, "%s %s", tystr(n->type), n->name); a->found = 1; }
    }
}

static char *read_whole(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *buf = NULL;
    size_t len = 0, cap = 0;
    for (;;) {
        if (len + 4096 + 1 > cap) { cap = cap ? cap * 2 : 8192; buf = realloc(buf, cap); if (!buf) { fclose(f); return NULL; } }
        size_t got = fread(buf + len, 1, cap - len - 1, f);
        if (!got) break;
        len += got;
    }
    fclose(f);
    buf[len] = 0;
    return buf;
}

/* where the declared name `name` stands at or after (line, col) of `path`: the declaration node starts at its modifiers or
 * type, so the identifier is looked up in the file's tokens */
static void refine_name(const char *path, const char *self_path, const char *self_src, int *line, int *col, const char *name, int *len) {
    *len = name ? (int)strlen(name) : 1;
    if (!path || !name) return;
    char *own = NULL;
    const char *src = self_src;
    if (!self_path || strcmp(path, self_path) != 0) { own = read_whole(path); src = own; }
    if (!src) return;
    const char *ls = line_start(src, *line - 1);
    if (ls) {
        /* the first whole-word occurrence of the name from the node's column on, on its line and the next few */
        int l = *line;
        const char *p = ls;
        for (int tries = 0; tries < 6 && *p; tries++, l++) {
            int n = line_len(p);
            int from = tries == 0 ? *col - 1 : 0;
            size_t nl = strlen(name);
            for (int i = from < 0 ? 0 : from; i + (int)nl <= n; i++) {
                if (memcmp(p + i, name, nl) == 0 && (i == 0 || !is_word(p[i - 1])) && (i + (int)nl == n || !is_word(p[i + nl]))) {
                    *line = l;
                    *col = i + 1;
                    free(own);
                    return;
                }
            }
            const char *nx = strchr(p, '\n');
            if (!nx) break;
            p = nx + 1;
        }
    }
    free(own);
}

/* runs in the child: prints one JSON value to stdout */
static int analyse(int mode, const char *path, const char *src, int line, int col) {
    if (mode == Q_DIAG) {
        j_diag_json();
        Node *head = jparse_unit(path, src);
        jt_init_default_roots(path, head->name);
        jt_load_unit(path, src);
        jt_check_all();
        if (!j_error_count()) jown_check_project(path);
        return j_error_count() ? 1 : 0;              /* the JSON document is written by the exit handler */
    }
    jparse_capture_path = path;
    Node *head = jparse_unit(path, src);
    jt_init_default_roots(path, head->name);
    jt_load_unit(path, src);
    jt_check_all();
    /* the identifier token under the cursor */
    JToken *tk = NULL;
    for (int i = 0; i < jparse_captured.count; i++) {
        JToken *t = &jparse_captured.toks[i];
        if (t->kind != JT_IDENT || t->line != line) continue;
        int len = (int)strlen(t->text);
        if (t->col <= col && col <= t->col + len) { tk = t; if (col < t->col + len) break; }
    }
    if (!tk) { json_none(); return 0; }
    Search s = { tk->text, tk->line, tk->col, (int)strlen(tk->text), {0}, {0}, {0}, 0, path };
    JClass **all;
    int n = jt_all_classes(&all);
    for (int i = 0; i < n; i++) {
        JClass *c = all[i];
        if (c->outer || !c->path || strcmp(c->path, path) != 0 || !c->decl) continue;
        walk(&s, c->decl, c, NULL, 0);
    }
    Answer a = {0};
    /* a use of the name says more than the bare token; try the hits in order */
    for (int i = 0; i < s.nhit && !a.found; i++) describe(&a, s.hit[i], s.hit_cls[i], s.hit_m[i]);
    if (!a.found) { json_none(); return 0; }
    JBuf out = {0};
    if (mode == Q_HOVER) {
        j_buf_add(&out, "{\"line\":");
        j_buf_addf(&out, "%d,\"col\":%d,\"len\":%d,\"text\":", tk->line, tk->col, (int)strlen(tk->text));
        js(&out, a.hover.data ? a.hover.data : "");
        j_buf_addc(&out, '}');
    } else {
        if (!a.def_path) { json_none(); return 0; }
        int dl = a.def_line, dc = a.def_col, dn;
        refine_name(a.def_path, path, src, &dl, &dc, a.def_name, &dn);
        j_buf_add(&out, "{\"file\":");
        js(&out, a.def_path);
        j_buf_addf(&out, ",\"line\":%d,\"col\":%d,\"len\":%d}", dl, dc, dn);
    }
    fputs(out.data, stdout);
    fputc('\n', stdout);
    fflush(stdout);
    return 0;
}

/* Runs one analysis in a child and returns what it wrote to stdout (malloc'd, NUL-terminated) or NULL when the child
 * died, ran out of time or wrote nothing. *status gets the exit status (-1 when it was killed). */
static char *run_child(int mode, const char *path, const char *src, int line, int col, int *status) {
    int fds[2];
    *status = -1;
    if (pipe(fds) != 0) return NULL;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return NULL; }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, STDIN_FILENO); dup2(dn, STDERR_FILENO); if (dn > 2) close(dn); }
        signal(SIGPIPE, SIG_DFL);
        struct rlimit lim = { ANALYSIS_MEMORY, ANALYSIS_MEMORY };
        setrlimit(RLIMIT_AS, &lim);
        const char *limit = getenv("ASH_LSP_SECONDS");               /* tests shorten it */
        int seconds = limit && atoi(limit) > 0 && atoi(limit) <= 600 ? atoi(limit) : ANALYSIS_SECONDS;
        lim.rlim_cur = lim.rlim_max = (rlim_t)seconds;
        setrlimit(RLIMIT_CPU, &lim);
        alarm((unsigned)seconds);
        char *dir = project_dir(path);
        if (dir) { if (chdir(dir) != 0) { /* stay where we are */ } free(dir); }
        int rc = analyse(mode, path, src, line, col);
        exit(rc);
    }
    close(fds[1]);
    JBuf out = {0};
    char buf[8192];
    for (;;) {
        ssize_t got = read(fds[0], buf, sizeof buf);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) break;
        if (out.len + (size_t)got > ANALYSIS_MAX_OUTPUT) { kill(pid, SIGKILL); break; }
        j_buf_addn(&out, buf, (size_t)got);
    }
    close(fds[0]);
    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
    if (WIFEXITED(st)) *status = WEXITSTATUS(st);
    if (!out.len) { free(out.data); return NULL; }
    return out.data;
}

/* ================================================================== documents */

#define MAX_DOCS 256

typedef struct {
    char *uri;
    char *path;
    char *text;
    int dirty;
    char **published;              /* uris that got diagnostics from the last analysis of this document */
    int npublished;
} Doc;

static Doc docs[MAX_DOCS];
static int ndocs;

static Doc *find_doc(const char *uri) {
    for (int i = 0; i < ndocs; i++) if (strcmp(docs[i].uri, uri) == 0) return &docs[i];
    return NULL;
}

static void free_published(Doc *d) {
    for (int i = 0; i < d->npublished; i++) free(d->published[i]);
    free(d->published);
    d->published = NULL;
    d->npublished = 0;
}

/* ================================================================== output */

static void send_body(const char *body, size_t len) {
    char head[64];
    int hn = snprintf(head, sizeof head, "Content-Length: %zu\r\n\r\n", len);
    const char *parts[2] = { head, body };
    size_t lens[2] = { (size_t)hn, len };
    for (int k = 0; k < 2; k++) {
        size_t off = 0;
        while (off < lens[k]) {
            ssize_t w = write(STDOUT_FILENO, parts[k] + off, lens[k] - off);
            if (w < 0 && errno == EINTR) continue;
            if (w <= 0) return;                      /* the client is gone: the read loop finds out */
            off += (size_t)w;
        }
    }
}

static void send_json(JBuf *b) { send_body(b->data, b->len); free(b->data); b->data = NULL; b->len = b->cap = 0; }

static void reply(const char *id_raw, const char *result_json) {
    JBuf b = {0};
    j_buf_addf(&b, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}", id_raw, result_json);
    send_json(&b);
}

static void reply_error(const char *id_raw, int code, const char *msg) {
    JBuf b = {0};
    j_buf_addf(&b, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":%d,\"message\":", id_raw, code);
    js(&b, msg);
    j_buf_add(&b, "}}");
    send_json(&b);
}

/* ================================================================== diagnostics */

static void range_json(JBuf *b, const char *text, int line1, int col1, int len_bytes) {
    int l0 = line1 > 0 ? line1 - 1 : 0;
    int c1 = col1 > 0 ? col1 : 1;
    int s = text ? to_lsp_char(text, l0 + 1, c1) : 0;
    int e = text ? to_lsp_char(text, l0 + 1, c1 + len_bytes) : s + 1;
    j_buf_addf(b, "{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}", l0, s, l0, e);
}

static void publish(const char *uri, const char *diags_json) {
    JBuf b = {0};
    j_buf_add(&b, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
    js(&b, uri);
    j_buf_addf(&b, ",\"diagnostics\":[%s]}}", diags_json ? diags_json : "");
    send_json(&b);
}

typedef struct { char *uri; JBuf items; int n; } Group;

static Group *group_for(Group **gs, int *ng, const char *uri) {
    for (int i = 0; i < *ng; i++) if (strcmp((*gs)[i].uri, uri) == 0) return &(*gs)[i];
    Group *ngp = realloc(*gs, (size_t)(*ng + 1) * sizeof(Group));
    if (!ngp) return NULL;
    *gs = ngp;
    Group *g = &ngp[(*ng)++];
    memset(g, 0, sizeof *g);
    g->uri = strdup(uri);
    return g;
}

/* the text of a file the diagnostics point into: the open document's, else the file on disk (the last one read is kept
 * for the next diagnostic that points into the same file; forget_text_cache drops it) */
static char *cache_path, *cache_text;

static void forget_text_cache(void) {
    free(cache_path);
    free(cache_text);
    cache_path = cache_text = NULL;
}

static char *text_of(const char *path, const Doc *self) {
    if (self && strcmp(self->path, path) == 0) return strdup(self->text);
    for (int i = 0; i < ndocs; i++) if (docs[i].path && strcmp(docs[i].path, path) == 0) return strdup(docs[i].text);
    if (cache_path && strcmp(cache_path, path) == 0) return cache_text ? strdup(cache_text) : NULL;
    forget_text_cache();
    cache_path = strdup(path);
    cache_text = read_whole(path);
    return cache_text ? strdup(cache_text) : NULL;
}

static void analyse_doc(Doc *d) {
    d->dirty = 0;
    int status;
    char *out = run_child(Q_DIAG, d->path, d->text, 0, 0, &status);
    Group *groups = NULL;
    int ng = 0;
    char *own_uri = path_to_uri(d->path);
    if (own_uri) group_for(&groups, &ng, own_uri);            /* the document is always published, empty when it has no diagnostics */
    free(own_uri);
    JP p;
    int parsed = out && jp_parse(&p, out, (int)strlen(out));
    int ok = parsed && jp_get(&p, 0, "diagnostics") >= 0;
    if (ok) {
        int arr = jp_get(&p, 0, "diagnostics");
        int i = arr + 1;
        for (int m = 0; m < p.toks[arr].size; m++) {
            int o = i;
            i = p.toks[o].next;
            char *file = jp_str(&p, jp_get(&p, o, "file"));
            char *msg = jp_str(&p, jp_get(&p, o, "message"));
            char *code = jp_str(&p, jp_get(&p, o, "code"));
            int line = (int)jp_int(&p, jp_get(&p, o, "line"), 0), col = (int)jp_int(&p, jp_get(&p, o, "column"), 0);
            const char *fpath = file && *file ? file : d->path;
            char *furi = path_to_uri(fpath);
            char *ftext = text_of(fpath, d);
            Group *g = furi ? group_for(&groups, &ng, furi) : NULL;
            if (g && msg) {
                JBuf *b = &g->items;
                if (g->n++) j_buf_addc(b, ',');
                j_buf_add(b, "{\"range\":");
                range_json(b, ftext, line, col, ftext ? word_at(ftext, line, col) : 1);
                j_buf_add(b, ",\"severity\":1,\"source\":\"ash\"");
                if (code) { j_buf_add(b, ",\"code\":"); js(b, code); }
                JBuf full = {0};
                j_buf_add(&full, msg);
                int help = jp_get(&p, o, "help");
                for (int h = 0, hi = help >= 0 ? help + 1 : 0; help >= 0 && h < p.toks[help].size; h++) {
                    char *hs = jp_str(&p, hi);
                    hi = p.toks[hi].next;
                    if (hs) { j_buf_add(&full, "\nhelp: "); j_buf_add(&full, hs); free(hs); }
                }
                j_buf_add(b, ",\"message\":");
                js(b, full.data);
                free(full.data);
                int notes = jp_get(&p, o, "notes");
                if (notes >= 0 && p.toks[notes].size > 0) {
                    j_buf_add(b, ",\"relatedInformation\":[");
                    int ni = notes + 1;
                    for (int k = 0; k < p.toks[notes].size; k++) {
                        int no = ni;
                        ni = p.toks[no].next;
                        char *nf = jp_str(&p, jp_get(&p, no, "file"));
                        char *nm = jp_str(&p, jp_get(&p, no, "message"));
                        int nl = (int)jp_int(&p, jp_get(&p, no, "line"), 1), nc = (int)jp_int(&p, jp_get(&p, no, "column"), 1);
                        const char *np = nf && *nf ? nf : fpath;
                        char *nu = path_to_uri(np);
                        char *nt = text_of(np, d);
                        if (k) j_buf_addc(b, ',');
                        j_buf_add(b, "{\"location\":{\"uri\":");
                        js(b, nu ? nu : "");
                        j_buf_add(b, ",\"range\":");
                        range_json(b, nt, nl, nc, nt ? word_at(nt, nl, nc) : 1);
                        j_buf_add(b, "},\"message\":");
                        js(b, nm ? nm : "");
                        j_buf_addc(b, '}');
                        free(nf); free(nm); free(nu); free(nt);
                    }
                    j_buf_addc(b, ']');
                }
                j_buf_addc(b, '}');
            }
            free(file); free(msg); free(code); free(furi); free(ftext);
        }
    } else {
        /* the analysis died or timed out: say so on the document instead of leaving stale answers */
        char *furi = path_to_uri(d->path);
        Group *g = furi ? group_for(&groups, &ng, furi) : NULL;
        if (g) {
            j_buf_add(&g->items, "{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":0}},\"severity\":2,\"source\":\"ash\",\"message\":");
            js(&g->items, status < 0 ? "the analysis of this file failed (the compiler crashed or took too long)" : "the analysis of this file produced no result");
            j_buf_addc(&g->items, '}');
            g->n++;
        }
        free(furi);
    }
    if (parsed) free(p.toks);
    free(out);
    forget_text_cache();
    /* publish what there is, and an empty list for every file that had diagnostics before and has none now */
    for (int i = 0; i < ng; i++) publish(groups[i].uri, groups[i].items.data);
    for (int i = 0; i < d->npublished; i++) {
        int still = 0;
        for (int k = 0; k < ng; k++) if (strcmp(groups[k].uri, d->published[i]) == 0) still = 1;
        if (!still) publish(d->published[i], "");
    }
    free_published(d);
    d->published = ng ? calloc((size_t)ng, sizeof(char *)) : NULL;
    for (int i = 0; d->published && i < ng; i++) d->published[d->npublished++] = strdup(groups[i].uri);
    for (int i = 0; i < ng; i++) { free(groups[i].uri); free(groups[i].items.data); }
    free(groups);
}

/* ================================================================== requests */

static void answer_query(const char *id_raw, int mode, Doc *d, int line0, int ch) {
    int col = to_byte_col(d->text, line0, ch);
    int status;
    char *out = run_child(mode, d->path, d->text, line0 + 1, col, &status);
    JP p;
    if (!out || !jp_parse(&p, out, (int)strlen(out)) || p.toks[0].type != J_OBJECT) {
        free(out);
        reply(id_raw, "null");
        return;
    }
    JBuf r = {0};
    if (mode == Q_HOVER) {
        char *text = jp_str(&p, jp_get(&p, 0, "text"));
        int l = (int)jp_int(&p, jp_get(&p, 0, "line"), 1), c = (int)jp_int(&p, jp_get(&p, 0, "col"), 1), n = (int)jp_int(&p, jp_get(&p, 0, "len"), 1);
        if (text) {
            j_buf_add(&r, "{\"contents\":{\"kind\":\"markdown\",\"value\":");
            JBuf md = {0};
            j_buf_add(&md, "```ash\n");
            j_buf_add(&md, text);
            j_buf_add(&md, "\n```");
            js(&r, md.data);
            free(md.data);
            j_buf_add(&r, "},\"range\":");
            range_json(&r, d->text, l, c, n);
            j_buf_addc(&r, '}');
        }
        free(text);
    } else {
        char *file = jp_str(&p, jp_get(&p, 0, "file"));
        int l = (int)jp_int(&p, jp_get(&p, 0, "line"), 1), c = (int)jp_int(&p, jp_get(&p, 0, "col"), 1), n = (int)jp_int(&p, jp_get(&p, 0, "len"), 1);
        char *text = file ? text_of(file, d) : NULL;           /* the library's built-in core is no file an editor could open */
        if (file && text) {
            char *uri = path_to_uri(file);
            j_buf_add(&r, "{\"uri\":");
            js(&r, uri ? uri : "");
            j_buf_add(&r, ",\"range\":");
            range_json(&r, text, l, c, n);
            j_buf_addc(&r, '}');
            free(uri);
        }
        free(text);
        free(file);
    }
    free(p.toks);
    free(out);
    forget_text_cache();
    reply(id_raw, r.data ? r.data : "null");
    free(r.data);
}

static int g_shutdown;

static void on_open(const JP *p, int params) {
    int td = jp_get(p, params, "textDocument");
    char *uri = jp_str(p, jp_get(p, td, "uri"));
    char *text = jp_str(p, jp_get(p, td, "text"));
    if (!uri || !text) { free(uri); free(text); return; }
    char *path = uri_to_path(uri);
    if (!path) { free(uri); free(text); return; }          /* only files */
    Doc *d = find_doc(uri);
    if (!d) {
        if (ndocs >= MAX_DOCS) { free(uri); free(text); free(path); return; }
        d = &docs[ndocs++];
        memset(d, 0, sizeof *d);
        d->uri = uri;
        d->path = path;
    } else {
        free(uri); free(path);
        free(d->text);
    }
    d->text = text;
    d->dirty = 1;
}

static void on_change(const JP *p, int params) {
    int td = jp_get(p, params, "textDocument");
    char *uri = jp_str(p, jp_get(p, td, "uri"));
    Doc *d = uri ? find_doc(uri) : NULL;
    free(uri);
    int ch = jp_get(p, params, "contentChanges");
    if (!d || ch < 0 || p->toks[ch].type != J_ARRAY || p->toks[ch].size == 0) return;
    /* full-document sync was announced: the last change holds the whole text. A change with a range (a client that
     * ignored the announcement) is skipped rather than guessed at. */
    int last = ch + 1;
    for (int i = 1; i < p->toks[ch].size; i++) last = p->toks[last].next;
    if (jp_get(p, last, "range") >= 0) return;
    char *text = jp_str(p, jp_get(p, last, "text"));
    if (!text) return;
    free(d->text);
    d->text = text;
    d->dirty = 1;
}

static void on_close(const JP *p, int params) {
    int td = jp_get(p, params, "textDocument");
    char *uri = jp_str(p, jp_get(p, td, "uri"));
    Doc *d = uri ? find_doc(uri) : NULL;
    if (d) {
        for (int i = 0; i < d->npublished; i++) publish(d->published[i], "");
        free_published(d);
        free(d->uri); free(d->path); free(d->text);
        *d = docs[--ndocs];
    }
    free(uri);
}

/* returns 1 for the exit notification */
static int handle(const char *body, int len) {
    JP p;
    if (!jp_parse(&p, body, len) || p.toks[0].type != J_OBJECT) {
        reply_error("null", -32700, "parse error");
        return 0;
    }
    char *method = jp_str(&p, jp_get(&p, 0, "method"));
    int idt = jp_get(&p, 0, "id");
    char *id_raw = NULL;
    if (idt >= 0 && p.toks[idt].type == J_PRIM) {                       /* a number */
        char *t = j_strndup(body + p.toks[idt].start, (size_t)(p.toks[idt].end - p.toks[idt].start));
        char *e;
        (void)strtod(t, &e);
        if (*t && !*e && strlen(t) < 40 && (isdigit((unsigned char)t[0]) || t[0] == '-')) id_raw = t; else free(t);
    } else if (idt >= 0 && p.toks[idt].type == J_STRING) {              /* a string: echoed in the form we would write it */
        char *str = jp_str(&p, idt);
        if (str) {
            JBuf ib = {0};
            js(&ib, str);
            id_raw = j_strndup(ib.data, ib.len);
            free(ib.data);
            free(str);
        }
    }
    int params = jp_get(&p, 0, "params");
    if (!method) {
        free(p.toks);                              /* a response from the client: nothing to do */
        free(id_raw);
        return 0;
    }
    if (!strcmp(method, "exit")) { free(method); free(id_raw); free(p.toks); return 1; }
    if (!strcmp(method, "initialize")) {
        int g = jp_get(&p, params, "capabilities");
        int gen = jp_get(&p, g, "general");
        int pe = jp_get(&p, gen, "positionEncodings");
        g_utf8 = 0;
        for (int i = 0, ti = pe >= 0 && p.toks[pe].type == J_ARRAY ? pe + 1 : 0; pe >= 0 && p.toks[pe].type == J_ARRAY && i < p.toks[pe].size; i++) {
            char *enc = jp_str(&p, ti);
            if (enc && !strcmp(enc, "utf-8")) g_utf8 = 1;
            free(enc);
            ti = p.toks[ti].next;
        }
        JBuf r = {0};
        j_buf_add(&r, "{\"capabilities\":{\"textDocumentSync\":{\"openClose\":true,\"change\":1,\"save\":{\"includeText\":false}},"
                      "\"hoverProvider\":true,\"definitionProvider\":true");
        if (g_utf8) j_buf_add(&r, ",\"positionEncoding\":\"utf-8\"");
        j_buf_add(&r, "},\"serverInfo\":{\"name\":\"ashvm lsp\"}}");
        if (id_raw) reply(id_raw, r.data);
        free(r.data);
    } else if (!strcmp(method, "shutdown")) {
        g_shutdown = 1;
        if (id_raw) reply(id_raw, "null");
    } else if (!strcmp(method, "textDocument/didOpen")) {
        on_open(&p, params);
    } else if (!strcmp(method, "textDocument/didChange")) {
        on_change(&p, params);
    } else if (!strcmp(method, "textDocument/didSave")) {
        char *uri = jp_str(&p, jp_get(&p, jp_get(&p, params, "textDocument"), "uri"));
        Doc *d = uri ? find_doc(uri) : NULL;
        if (d) d->dirty = 1;
        free(uri);
    } else if (!strcmp(method, "textDocument/didClose")) {
        on_close(&p, params);
    } else if (!strcmp(method, "textDocument/hover") || !strcmp(method, "textDocument/definition")) {
        int td = jp_get(&p, params, "textDocument"), pos = jp_get(&p, params, "position");
        char *uri = jp_str(&p, jp_get(&p, td, "uri"));
        Doc *d = uri ? find_doc(uri) : NULL;
        long line = jp_int(&p, jp_get(&p, pos, "line"), -1), ch = jp_int(&p, jp_get(&p, pos, "character"), -1);
        if (id_raw) {
            if (d && line >= 0 && line < INT_MAX && ch >= 0 && ch < INT_MAX) answer_query(id_raw, !strcmp(method, "textDocument/hover") ? Q_HOVER : Q_DEFINITION, d, (int)line, (int)ch);
            else reply(id_raw, "null");
        }
        free(uri);
    } else if (id_raw) {
        if (!strncmp(method, "textDocument/", 13) || !strncmp(method, "workspace/", 10)) reply(id_raw, "null");   /* asked for something we do not offer */
        else reply_error(id_raw, -32601, "method not found");
    }
    free(method);
    free(id_raw);
    free(p.toks);
    return 0;
}

/* ================================================================== input */

#define MAX_MESSAGE (64 << 20)

static char *inbuf;
static size_t inlen, incap;

static int fill(void) {
    if (inlen + 65536 > incap) {
        size_t ncap = incap ? incap * 2 : 1 << 17;
        char *nb = realloc(inbuf, ncap);
        if (!nb) return 0;
        inbuf = nb;
        incap = ncap;
    }
    for (;;) {
        ssize_t got = read(STDIN_FILENO, inbuf + inlen, incap - inlen);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) return 0;
        inlen += (size_t)got;
        return 1;
    }
}

static int input_pending(void) {
    if (inlen > 0) return 1;
    struct pollfd pf = { STDIN_FILENO, POLLIN, 0 };
    return poll(&pf, 1, 0) > 0;
}

/* Returns 1 with a malloc'd body, 0 at end of input. */
static int next_message(char **body, int *len) {
    for (;;) {
        char *hdr_end = NULL;
        for (size_t i = 0; i + 3 < inlen; i++) if (inbuf[i] == '\r' && inbuf[i + 1] == '\n' && inbuf[i + 2] == '\r' && inbuf[i + 3] == '\n') { hdr_end = inbuf + i; break; }
        if (!hdr_end) {
            if (inlen > 8192) { memmove(inbuf, inbuf + inlen - 3, 3); inlen = 3; }      /* no header ends in this much text: not LSP, resync */
            if (!fill()) return 0;
            continue;
        }
        long clen = -1;
        for (char *l = inbuf; l < hdr_end;) {
            char *e = memchr(l, '\n', (size_t)(hdr_end + 2 - l));
            if (!e) break;
            if ((size_t)(e - l) > 15 && strncasecmp(l, "Content-Length:", 15) == 0) clen = strtol(l + 15, NULL, 10);
            l = e + 1;
        }
        size_t hlen = (size_t)(hdr_end - inbuf) + 4;
        if (clen < 0 || clen > MAX_MESSAGE) {
            memmove(inbuf, inbuf + hlen, inlen - hlen);               /* a header we cannot use: drop it and go on */
            inlen -= hlen;
            if (clen > MAX_MESSAGE) {                                 /* ...and the body, which we will not read into memory */
                long skip = clen;
                while (skip > 0) {
                    if (inlen == 0 && !fill()) return 0;
                    size_t take = (size_t)skip < inlen ? (size_t)skip : inlen;
                    memmove(inbuf, inbuf + take, inlen - take);
                    inlen -= take;
                    skip -= (long)take;
                }
            }
            continue;
        }
        while (inlen < hlen + (size_t)clen) if (!fill()) return 0;
        char *b = malloc((size_t)clen + 1);
        if (!b) return 0;
        memcpy(b, inbuf + hlen, (size_t)clen);
        b[clen] = 0;
        memmove(inbuf, inbuf + hlen + (size_t)clen, inlen - hlen - (size_t)clen);
        inlen -= hlen + (size_t)clen;
        *body = b;
        *len = (int)clen;
        return 1;
    }
}

int jlsp_serve(void) {
    signal(SIGPIPE, SIG_IGN);
    for (;;) {
        char *body;
        int len;
        if (!next_message(&body, &len)) return g_shutdown ? 0 : 1;
        int is_exit = handle(body, len);
        free(body);
        if (is_exit) return g_shutdown ? 0 : 1;
        /* analyse once the editor has gone quiet: while more input is waiting, only collect it */
        if (!input_pending()) {
            for (int i = 0; i < ndocs; i++) if (docs[i].dirty) analyse_doc(&docs[i]);
        }
    }
}
