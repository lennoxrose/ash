#include "jcommon/H/jcommon.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void *j_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) {
        fprintf(stderr, "error: out of memory\n");
        exit(1);
    }
    return p;
}

char *j_strndup(const char *s, size_t n) {
    char *p = j_alloc(n + 1);
    memcpy(p, s, n);
    return p;
}

char *j_strdup(const char *s) { return j_strndup(s, strlen(s)); }

char *j_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *p = j_alloc((size_t)n + 1);
    vsnprintf(p, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return p;
}

void j_list_push(JList *l, void *item) {
    if (l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 4;
        l->items = realloc(l->items, sizeof(void *) * (size_t)l->cap);
        if (!l->items) {
            fprintf(stderr, "error: out of memory\n");
            exit(1);
        }
    }
    l->items[l->count++] = item;
}

static void buf_reserve(JBuf *b, size_t extra) {
    if (b->len + extra + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        while (cap < b->len + extra + 1) cap *= 2;
        b->data = realloc(b->data, cap);
        if (!b->data) {
            fprintf(stderr, "error: out of memory\n");
            exit(1);
        }
        b->cap = cap;
    }
}

void j_buf_addn(JBuf *b, const char *s, size_t n) {
    buf_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = 0;
}

void j_buf_add(JBuf *b, const char *s) { j_buf_addn(b, s, strlen(s)); }

void j_buf_addc(JBuf *b, char c) { j_buf_addn(b, &c, 1); }

void j_buf_addf(JBuf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    buf_reserve(b, (size_t)n);
    vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)n;
}

typedef struct {
    const char *path;
    const char *text;
} SourceEntry;

static SourceEntry sources[256];
static int source_count;
static int errors;

void j_register_source(const char *path, const char *text) {
    if (source_count < 256) {
        sources[source_count].path = path;
        sources[source_count].text = text;
        source_count++;
    }
}

static const char *find_source(const char *path) {
    for (int i = 0; i < source_count; i++) {
        if (strcmp(sources[i].path, path) == 0) return sources[i].text;
    }
    return NULL;
}

#define COL_RED   "\033[38;5;203m"
#define COL_CYAN  "\033[38;5;117m"
#define COL_GRAY  "\033[38;5;242m"
#define COL_RESET "\033[0m"
#define COL_BOLD  "\033[1m"

static int use_color(void) {
    return isatty(STDERR_FILENO) && !getenv("NO_COLOR");
}

/* ---- extra lines of the next diagnostic: help text and secondary locations ---- */

#define MAX_EXTRA 6
typedef struct {
    int is_note;       /* 1: a note that points at a second place in the source; 0: a plain help line */
    const char *path;
    int line, col;
    char *msg;
} Extra;
static Extra extras[MAX_EXTRA];
static int extra_count;
static const char *pending_code;

static void add_extra(int is_note, const char *path, int line, int col, const char *fmt, va_list ap) {
    if (extra_count >= MAX_EXTRA) return;
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    char *m = j_alloc((size_t)n + 1);
    vsnprintf(m, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    extras[extra_count++] = (Extra){is_note, path, line, col, m};
}

void j_diag_help(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    add_extra(0, NULL, 0, 0, fmt, ap);
    va_end(ap);
}

void j_diag_note(const char *path, int line, int col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    add_extra(1, path, line, col, fmt, ap);
    va_end(ap);
}

void j_diag_code(const char *code) { pending_code = code; }

/* ---- JSON mode (`ashvm check --json`, `kiln --json`): one document at exit instead of text on stderr ---- */

static int json_mode;
static JBuf json_items;
static int json_count;
static int json_registered;

static void json_str(JBuf *b, const char *s) {
    j_buf_addc(b, '"');
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { j_buf_addc(b, '\\'); j_buf_addc(b, (char)c); }
        else if (c == '\n') j_buf_add(b, "\\n");
        else if (c == '\r') j_buf_add(b, "\\r");
        else if (c == '\t') j_buf_add(b, "\\t");
        else if (c < 0x20) j_buf_addf(b, "\\u%04x", c);
        else j_buf_addc(b, (char)c);
    }
    j_buf_addc(b, '"');
}

static void json_flush(void) {
    printf("{\"diagnostics\":[%s],\"errors\":%d}\n", json_items.data ? json_items.data : "", json_count);
    fflush(stdout);
}

void j_diag_json(void) {
    json_mode = 1;
    if (!json_registered) { json_registered = 1; atexit(json_flush); }
}

int j_diag_is_json(void) { return json_mode; }

static void json_report(const char *path, int line, int col, const char *fmt, va_list ap) {
    JBuf *b = &json_items;
    if (json_count++) j_buf_addc(b, ',');
    j_buf_add(b, "{\"severity\":\"error\",\"code\":");
    if (pending_code) json_str(b, pending_code); else j_buf_add(b, "null");
    j_buf_add(b, ",\"message\":");
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    char *m = j_alloc((size_t)n + 1);
    vsnprintf(m, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    json_str(b, m);
    j_buf_add(b, ",\"file\":");
    json_str(b, path ? path : "");
    j_buf_addf(b, ",\"line\":%d,\"column\":%d,\"help\":[", line, col);
    int first = 1;
    for (int i = 0; i < extra_count; i++) {
        if (extras[i].is_note) continue;
        if (!first) j_buf_addc(b, ',');
        first = 0;
        json_str(b, extras[i].msg);
    }
    j_buf_add(b, "],\"notes\":[");
    first = 1;
    for (int i = 0; i < extra_count; i++) {
        if (!extras[i].is_note) continue;
        if (!first) j_buf_addc(b, ',');
        first = 0;
        j_buf_add(b, "{\"message\":");
        json_str(b, extras[i].msg);
        j_buf_add(b, ",\"file\":");
        json_str(b, extras[i].path ? extras[i].path : "");
        j_buf_addf(b, ",\"line\":%d,\"column\":%d}", extras[i].line, extras[i].col);
    }
    j_buf_add(b, "]}");
}

/* the source line `line` of `path` and a caret under `col`, in the frame every diagnostic uses */
static void snippet(const char *path, int line, int col, const char *gray, const char *red, const char *reset) {
    const char *text = path ? find_source(path) : NULL;
    if (!text || line <= 0) return;
    const char *p = text;
    for (int l = 1; l < line && *p; p++) {
        if (*p == '\n') l++;
    }
    const char *end = p;
    while (*end && *end != '\n') end++;
    fprintf(stderr, "%s   |%s\n%s%3d|%s %.*s\n%s   |%s ", gray, reset, gray, line, reset, (int)(end - p), p, gray, reset);
    for (int i = 1; i < col; i++) fputc(' ', stderr);
    fprintf(stderr, "%s^%s\n", red, reset);
}

static void report(const char *path, int line, int col, const char *fmt, va_list ap) {
    if (json_mode) {
        json_report(path, line, col, fmt, ap);
        extra_count = 0;
        pending_code = NULL;
        return;
    }
    int color = use_color();
    const char *bold = color ? COL_BOLD : "", *red = color ? COL_RED : "", *cyan = color ? COL_CYAN : "";
    const char *gray = color ? COL_GRAY : "", *reset = color ? COL_RESET : "";
    if (pending_code) fprintf(stderr, "%s%serror[%s]%s%s: ", bold, red, pending_code, reset, bold);
    else fprintf(stderr, "%s%serror%s%s: ", bold, red, reset, bold);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "%s\n%s  --> %s:%d:%d%s\n", reset, cyan, path ? path : "<unknown>", line, col, reset);
    snippet(path, line, col, gray, red, reset);
    for (int i = 0; i < extra_count; i++) {
        Extra *x = &extras[i];
        if (!x->is_note) {
            fprintf(stderr, "%s   = %shelp%s: %s\n", gray, bold, reset, x->msg);
            continue;
        }
        fprintf(stderr, "%s   = %snote%s: %s\n", gray, bold, reset, x->msg);
        if (x->path && x->line > 0) {
            fprintf(stderr, "%s  --> %s:%d:%d%s\n", cyan, x->path, x->line, x->col, reset);
            snippet(x->path, x->line, x->col, gray, cyan, reset);
        }
    }
    extra_count = 0;
    pending_code = NULL;
}

void j_error(const char *path, int line, int col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    report(path, line, col, fmt, ap);
    va_end(ap);
    exit(1);
}

void j_error_soft(const char *path, int line, int col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    report(path, line, col, fmt, ap);
    va_end(ap);
    errors++;
}

void j_abort_if_errors(void) {
    if (errors) {
        if (json_mode) exit(1);
        int color = use_color();
        fprintf(stderr, "%s%d error%s%s\n", color ? COL_BOLD COL_RED : "", errors, errors == 1 ? "" : "s", color ? COL_RESET : "");
        exit(1);
    }
}

int j_error_count(void) { return errors; }

char *j_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = j_alloc((size_t)n + 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return NULL;
    }
    fclose(f);
    return buf;
}
