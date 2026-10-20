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

/* Extra lines for the NEXT diagnostic reported (error or soft error); they are dropped once it is printed.
 * j_diag_help adds `= help: ..`; j_diag_note adds `= note: ..` that points at a second place in the source
 * (the line and a caret under `col`, e.g. where a value was moved); j_diag_code names it (`E0101`). */
void j_diag_help(const char *fmt, ...);
void j_diag_note(const char *path, int line, int col, const char *fmt, ...);
void j_diag_code(const char *code);

/* JSON mode (ashvm check --json, kiln --json): diagnostics are collected and written to stdout as one
 * document {"diagnostics":[{severity, code, message, file, line, column, help[], notes[]}], "errors":N}
 * when the process exits; nothing goes to stderr. */
void j_diag_json(void);
int j_diag_is_json(void);

char *j_read_file(const char *path); /* NULL when unreadable */

#endif
