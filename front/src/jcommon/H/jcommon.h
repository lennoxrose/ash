#ifndef JCOMMON_H
#define JCOMMON_H

#include <stddef.h>

/* Shared utilities for the new-Ash front end (lexer, parser, checker, lowering).
 * Memory: everything is arena-allocated and never freed; the front end runs once per
 * compilation and the process exits afterwards. Errors: j_error reports a Rust-style
 * diagnostic and aborts the compilation (exit status 1). */

void *j_alloc(size_t n);                 /* zeroed */
char *j_strndup(const char *s, size_t n);
char *j_strdup(const char *s);
char *j_fmt(const char *fmt, ...);

/* Growable pointer list. */
typedef struct {
    void **items;
    int count;
    int cap;
} JList;

void j_list_push(JList *l, void *item);

/* Growable string buffer. */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} JBuf;

void j_buf_add(JBuf *b, const char *s);
void j_buf_addn(JBuf *b, const char *s, size_t n);
void j_buf_addf(JBuf *b, const char *fmt, ...);
void j_buf_addc(JBuf *b, char c);

/* Source registry for diagnostics. */
void j_register_source(const char *path, const char *text);
void j_error(const char *path, int line, int col, const char *fmt, ...);
/* Non-fatal: counts the error; call j_abort_if_errors at the end of a phase. */
void j_error_soft(const char *path, int line, int col, const char *fmt, ...);
void j_abort_if_errors(void);
int j_error_count(void);

char *j_read_file(const char *path); /* NULL when unreadable */

#endif
