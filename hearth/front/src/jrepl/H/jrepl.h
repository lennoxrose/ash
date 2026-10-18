#ifndef JREPL_H
#define JREPL_H

/* Session model for the ashvm REPL (MIGRATION.md D9: the REPL is an explicit exception to the
 * class-per-file rule; each accepted line is wrapped into a hidden class body).
 *
 * A session is the accepted history: imports, type declarations, static methods and statements. To run a
 * line, jrepl_source() builds a complete program in which the history statements are replayed, then a
 * marker line is printed, then the new line runs. The caller discards everything the program prints
 * before JREPL_MARKER, so only the new line's output is shown. Because state is rebuilt by replay,
 * non-idempotent side effects of earlier lines (file writes, stdin reads, clocks) happen again.
 *
 * Ownership: strings live in the front end's arena and are never freed. A session holds no OS
 * resources and nothing here terminates the process; compile errors surface only when the returned
 * source is compiled in a child process. */

#define JREPL_MARKER "\x01" "ash-repl" "\x01"

typedef enum {
    JREPL_IMPORT,     /* import ash.util.List; */
    JREPL_TYPE,       /* class / interface / enum / record declaration */
    JREPL_METHOD,     /* Type name(params) { ... } becomes a static method of the hidden class */
    JREPL_STATEMENT,  /* ends with ';' or '}' */
    JREPL_EXPRESSION  /* anything else: evaluated and printed */
} JReplKind;

typedef struct JRepl JRepl;

JRepl *jrepl_new(void);
JReplKind jrepl_classify(const char *line);

/* Program text that replays the history and then runs `line`. For JREPL_EXPRESSION, `print_value`
 * selects between printing the value and running it as a statement (for void calls). */
char *jrepl_source(JRepl *r, const char *line, JReplKind kind, int print_value);

/* Adds an accepted line to the history; call only after jrepl_source() ran without error. */
void jrepl_commit(JRepl *r, const char *line, JReplKind kind, int print_value);

#endif
