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

static void report(const char *path, int line, int col, const char *fmt, va_list ap) {
    int color = use_color();
    const char *bold = color ? COL_BOLD : "", *red = color ? COL_RED : "", *cyan = color ? COL_CYAN : "";
    const char *gray = color ? COL_GRAY : "", *reset = color ? COL_RESET : "";
    fprintf(stderr, "%s%serror%s%s: ", bold, red, reset, bold);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "%s\n%s  --> %s:%d:%d%s\n", reset, cyan, path ? path : "<unknown>", line, col, reset);
    const char *text = path ? find_source(path) : NULL;
    if (text && line > 0) {
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
