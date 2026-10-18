#include <ctype.h>
#include <string.h>

#include "jcommon/H/jcommon.h"
#include "jrepl/H/jrepl.h"

struct JRepl {
    JBuf imports;
    JBuf types;
    JBuf methods;
    JBuf stmts;
};

JRepl *jrepl_new(void) { return j_alloc(sizeof(JRepl)); }

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int is_ident_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

/* length of the identifier-or-keyword at p, 0 if none */
static size_t word_len(const char *p) {
    size_t n = 0;
    while (is_ident_char(p[n])) n++;
    return n;
}

static int word_is(const char *p, size_t n, const char *w) { return strlen(w) == n && strncmp(p, w, n) == 0; }

static int is_modifier(const char *p, size_t n) {
    static const char *mods[] = { "public", "private", "protected", "static", "final", "abstract", "sealed", "native" };
    for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++)
        if (word_is(p, n, mods[i])) return 1;
    return 0;
}

static int is_statement_keyword(const char *p, size_t n) {
    static const char *kws[] = { "new", "return", "throw", "else", "case", "default", "if", "while", "for", "do",
                                 "switch", "try", "break", "continue", "this", "super", "var" };
    for (size_t i = 0; i < sizeof(kws) / sizeof(kws[0]); i++)
        if (word_is(p, n, kws[i])) return 1;
    return 0;
}

/* Skips a type reference at p (qualified name, balanced <...>, [] pairs, trailing ?); NULL if p is not one. */
static const char *skip_type(const char *p) {
    size_t n = word_len(p);
    if (!n) return NULL;
    p += n;
    while (*p == '.' && word_len(p + 1)) p += 1 + word_len(p + 1);
    if (*p == '<') {
        int depth = 0;
        for (; *p; p++) {
            if (*p == '<') depth++;
            else if (*p == '>' && --depth == 0) { p++; break; }
            else if (!(is_ident_char(*p) || strchr(" ,.?[]&", *p))) return NULL;
        }
        if (depth) return NULL;
    }
    for (;;) {
        const char *q = skip_ws(p);
        if (q[0] == '[' && skip_ws(q + 1)[0] == ']') { p = skip_ws(q + 1) + 1; continue; }
        if (q[0] == '?') { p = q + 1; continue; }
        break;
    }
    return p;
}

static int ends_with_terminator(const char *line) {
    size_t n = strlen(line);
    while (n && isspace((unsigned char)line[n - 1])) n--;
    return n && (line[n - 1] == ';' || line[n - 1] == '}');
}

JReplKind jrepl_classify(const char *line) {
    const char *p = skip_ws(line);
    size_t n = word_len(p);
    if (word_is(p, n, "import")) return JREPL_IMPORT;
    const char *q = p;
    while (n && is_modifier(q, n)) { q = skip_ws(q + n); n = word_len(q); }
    if (word_is(q, n, "class") || word_is(q, n, "interface") || word_is(q, n, "enum")) return JREPL_TYPE;
    if (word_is(q, n, "record") && word_len(skip_ws(q + n))) return JREPL_TYPE;
    if (n && !is_statement_keyword(q, n)) {
        const char *after = skip_type(q);
        if (after && after != q && isspace((unsigned char)*after)) {
            const char *name = skip_ws(after);
            size_t nl = word_len(name);
            if (nl && skip_ws(name + nl)[0] == '(') return JREPL_METHOD;
        }
    }
    return ends_with_terminator(line) ? JREPL_STATEMENT : JREPL_EXPRESSION;
}

static char *strip_semicolons(const char *line) {
    char *s = j_strdup(line);
    size_t n = strlen(s);
    while (n && (isspace((unsigned char)s[n - 1]) || s[n - 1] == ';')) s[--n] = 0;
    return s;
}

/* the text a line contributes to the hidden class or to main */
static char *body_of(const char *line, JReplKind kind, int print_value) {
    if (kind == JREPL_EXPRESSION) {
        char *e = strip_semicolons(line);
        return print_value ? j_fmt("System.out.println(%s);\n", e) : j_fmt("%s;\n", e);
    }
    if (kind == JREPL_METHOD) {
        const char *p = skip_ws(line);
        for (;;) {
            size_t n = word_len(p);
            if (word_is(p, n, "static")) return j_fmt("%s\n", line);
            if (!is_modifier(p, n)) break;
            p = skip_ws(p + n);
        }
        return j_fmt("static %s\n", line);
    }
    return j_fmt("%s\n", line);
}

char *jrepl_source(JRepl *r, const char *line, JReplKind kind, int print_value) {
    JBuf out = {0};
    char *body = body_of(line, kind, print_value);
    if (kind == JREPL_IMPORT) j_buf_add(&out, body);
    j_buf_add(&out, r->imports.data ? r->imports.data : "");
    j_buf_add(&out, r->types.data ? r->types.data : "");
    if (kind == JREPL_TYPE) j_buf_add(&out, body);
    j_buf_add(&out, "class Repl {\n");
    j_buf_add(&out, r->methods.data ? r->methods.data : "");
    if (kind == JREPL_METHOD) j_buf_add(&out, body);
    j_buf_add(&out, "public static void main(String[] args) {\n");
    j_buf_add(&out, r->stmts.data ? r->stmts.data : "");
    j_buf_add(&out, "System.out.println(\"\\x01ash-repl\\x01\");\n");
    if (kind == JREPL_STATEMENT || kind == JREPL_EXPRESSION) j_buf_add(&out, body);
    j_buf_add(&out, "}\n}\n");
    return out.data;
}

void jrepl_commit(JRepl *r, const char *line, JReplKind kind, int print_value) {
    char *body = body_of(line, kind, print_value);
    switch (kind) {
        case JREPL_IMPORT: j_buf_add(&r->imports, body); break;
        case JREPL_TYPE: j_buf_add(&r->types, body); break;
        case JREPL_METHOD: j_buf_add(&r->methods, body); break;
        default: j_buf_add(&r->stmts, body); break;
    }
}
