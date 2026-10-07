#ifndef ASH_DIAGNOSTICS_H
#define ASH_DIAGNOSTICS_H

// Rust-style colorized error reporting, shared by every stage (compile-time
// and runtime) that needs to point at a spot in the original source text.

void diagnostics_set_source(const char *source, const char *filename);

// pos/len locate the offending span in the source buffer passed to
// diagnostics_set_source(); len <= 0 is treated as a single-character span.
void diagnostics_report(const char *label, const char *message,
                         const char *pos, int len);

typedef struct {
    const char *source;
    const char *filename;
} DiagnosticsSource;

// Saves the currently-active (source, filename) pair, so stmt_import.c
// can point diagnostics at an imported file's own buffer while
// recursively parsing it, then restore the importing file's pair
// afterward -- mirrors lexer.h's LexerState save/restore.
DiagnosticsSource diagnostics_save_source(void);
void diagnostics_restore_source(DiagnosticsSource saved);

#endif
