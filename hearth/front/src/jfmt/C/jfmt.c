#include <string.h>

#include "jcommon/H/jcommon.h"
#include "jfmt/H/jfmt.h"
#include "jlex/H/jlex.h"

#define MAX_DEPTH 256

typedef struct {
    int in_block_comment;
    int brace;                  /* open { */
    int paren;                  /* open ( and [ */
    char case_body[MAX_DEPTH];  /* per brace level: inside a colon-form `case`, so statements are one level deeper */
    char prev_last;             /* last code character of the previous code line, 0 at the start */
    int delta;                  /* new indent - old indent of the last line that started a statement */
    int blank_run;
    int too_deep;
} State;

/* skips a string literal starting at *pp (on the opening quote), including ${...} parts with nested strings */
static void skip_string(const char **pp) {
    const char *p = *pp + 1;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) { p += 2; continue; }
        if (*p == '$' && p[1] == '{') {
            int depth = 1;
            p += 2;
            while (*p && depth > 0) {
                if (*p == '"') { skip_string(&p); continue; }
                if (*p == '\'' ) { p++; if (*p == '\\') p++; if (*p) p++; if (*p == '\'') p++; continue; }
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }
            continue;
        }
        p++;
    }
    *pp = *p ? p + 1 : p;
}

typedef struct {
    int starts_close;       /* the code starts with } */
    char last;              /* last code character, 0 for a line without code */
    int comment_only;       /* nothing but comment on the line */
    int case_label;         /* starts with `case` or `default` */
    int colon_form;         /* ... and it uses ':' (no top-level '->'): the statements that follow belong to it */
    int starts_paren;       /* the first code character is ) or ] */
    int starts_in_comment;  /* the line begins inside a block comment */
} Line;

/* after the word `default`: ':' or '->' (a switch label), not a method modifier */
static int default_label(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return *p == ':' || (p[0] == '-' && p[1] == '>');
}

/* Reads one line (outside any string it skips). apply == 0 classifies the line and tracks block comments;
 * apply == 1 reads it again from the same start and applies its braces and parentheses to the state in order. */
static void scan_line(State *st, const char *body, Line *ln, int apply) {
    int in_block = apply ? ln->starts_in_comment : st->in_block_comment;
    int pd = 0, arrow = 0;
    if (!apply) { memset(ln, 0, sizeof *ln); ln->starts_in_comment = in_block; }
    const char *p = body;
    int seen_code = 0, seen_any = 0;
    while (*p) {
        if (in_block) {
            const char *e = strstr(p, "*/");
            if (!e) { p += strlen(p); break; }
            in_block = 0;
            p = e + 2;
            seen_any = 1;
            continue;
        }
        char c = *p;
        if (c == ' ' || c == '\t') { p++; continue; }
        if (c == '/' && p[1] == '/') { seen_any = 1; break; }
        if (c == '/' && p[1] == '*') { in_block = 1; p += 2; seen_any = 1; continue; }
        if (!seen_code) {
            seen_code = 1;
            if (!apply) {
                if (c == '}') ln->starts_close = 1;
                if (c == ')' || c == ']') ln->starts_paren = 1;
                if ((!strncmp(p, "case", 4) && (p[4] == ' ' || p[4] == '(' || p[4] == '\t')) ||
                    (!strncmp(p, "default", 7) && default_label(p + 7)))
                    ln->case_label = 1;
            }
        }
        seen_any = 1;
        if (c == '"') { skip_string(&p); if (!apply) ln->last = '"'; continue; }
        if (c == '\'') {
            p++;
            if (*p == '\\') p++;
            if (*p) p++;
            if (*p == '\'') p++;
            if (!apply) ln->last = '\'';
            continue;
        }
        if (apply) {
            if (c == '{') { if (st->brace + 1 < MAX_DEPTH) st->brace++; else st->too_deep = 1; st->case_body[st->brace] = 0; }
            else if (c == '}') { if (st->brace > 0) st->brace--; }
            else if (c == '(' || c == '[') st->paren++;
            else if (c == ')' || c == ']') { if (st->paren > 0) st->paren--; }
        } else {
            ln->last = c;
            if (c == '(' || c == '[' || c == '{') pd++;
            else if (c == ')' || c == ']' || c == '}') pd--;
            else if (c == '-' && p[1] == '>' && pd == 0) arrow = 1;
        }
        p++;
    }
    if (!apply) {
        ln->comment_only = seen_any && !seen_code;
        if (ln->case_label && !arrow && ln->last != '{') ln->colon_form = 1;   /* `case 1: {` opens its block on the label line */
        st->in_block_comment = in_block;
    }
}

static int indent_width(const char *line, const char **rest) {
    int w = 0;
    const char *p = line;
    for (;; p++) {
        if (*p == ' ') w++;
        else if (*p == '\t') w += 4;
        else break;
    }
    *rest = p;
    return w;
}

static char *format_text(const char *src, const char **err) {
    State st;
    memset(&st, 0, sizeof st);
    JBuf out = {0};
    const char *p = src;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char *raw = j_strndup(p, len);
        p = nl ? nl + 1 : p + len;
        while (len > 0 && (raw[len - 1] == '\r' || raw[len - 1] == ' ' || raw[len - 1] == '\t')) raw[--len] = 0;
        const char *body;
        int old_indent = indent_width(raw, &body);
        if (!*body) {                                   /* blank */
            if (st.blank_run == 0 && out.len > 0) j_buf_addc(&out, '\n');
            st.blank_run++;
            continue;
        }
        int carried = st.in_block_comment;
        Line ln;
        scan_line(&st, body, &ln, 0);
        int indent;
        if (carried) {                                  /* inside a block comment: the text is kept, shifted with the comment */
            indent = old_indent + st.delta;
        } else {
            int level = st.brace - (ln.starts_close ? 1 : 0);
            if (level < 0) level = 0;
            int is_start = st.paren <= 0 && !ln.starts_paren &&
                           (st.prev_last == 0 || st.prev_last == ';' || st.prev_last == '{' || st.prev_last == '}' ||
                            st.prev_last == ':' || st.prev_last == ',');
            if (is_start || ln.starts_close) {
                /* one level deeper for every colon-form `case` body the line is nested in (a case label itself only
                 * counts the cases of the switches around it) */
                int extra = 0;
                for (int l = 0; l < level || (l == level && !ln.case_label); l++) if (st.case_body[l]) extra += 4;
                indent = level * 4 + extra;
                st.delta = indent - old_indent;
            } else {
                indent = old_indent + st.delta;
            }
        }
        if (indent < 0) indent = 0;
        for (int i = 0; i < indent; i++) j_buf_addc(&out, ' ');
        j_buf_add(&out, body);
        j_buf_addc(&out, '\n');
        st.blank_run = 0;
        if (ln.case_label) st.case_body[st.brace] = ln.colon_form ? 1 : 0;
        scan_line(&st, body, &ln, 1);
        if (st.too_deep) { *err = "nesting is too deep to format"; return NULL; }
        if (ln.last) st.prev_last = ln.last;
    }
    if (out.len == 0) return j_strdup("");
    while (out.len > 1 && out.data[out.len - 1] == '\n' && out.data[out.len - 2] == '\n') out.data[--out.len] = 0;
    return out.data;
}

/* the same tokens (kind and text) in both texts? */
static int same_tokens(const char *path, const char *a, const char *b) {
    JTokens ta = jlex_tokenize(path, a);
    JTokens tb = jlex_tokenize(path, b);
    if (ta.count != tb.count) return 0;
    for (int i = 0; i < ta.count; i++) {
        if (ta.toks[i].kind != tb.toks[i].kind || strcmp(ta.toks[i].text, tb.toks[i].text) != 0) return 0;
    }
    return 1;
}

char *jfmt_format(const char *path, const char *src, const char **err) {
    *err = NULL;
    char *out = format_text(src, err);
    if (!out) return NULL;
    if (!same_tokens(path, src, out)) {
        *err = "formatting would change the program (internal error); the file was left as it is";
        return NULL;
    }
    return out;
}
