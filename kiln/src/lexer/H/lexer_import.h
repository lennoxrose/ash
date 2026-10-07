#ifndef KILN_LEXER_IMPORT_H
#define KILN_LEXER_IMPORT_H
#include "lexer/H/lexer.h"

// Cross-file link between lexer.c (the tokenizer's cursor state and its
// low-level scan primitives) and lexer_import.c (state save/restore plus
// import-path scanning, split out here to keep lexer.c under the
// project's ~200-line guideline) -- not part of the public API. The
// public entry points the two files implement together
// (lexer_save_state/lexer_restore_state/lexer_scan_import_path) stay
// declared in lexer.h exactly as before, so callers outside the lexer
// directory don't need to change anything. Mirrors parser.c's split
// between parser.h (public surface) and parser_internal.h (private,
// cross-file plumbing).
//
// Named lex_* rather than the bare start/current/line: parser.h already
// declares its own global `Token current`, and since these now need
// external linkage to be shared across the two .c files, a second plain
// `current` global here would collide with it at link time.
extern const char *lex_start;
extern const char *lex_current;
extern int lex_line;

int is_at_end(void);
char advance(void);
char peek(void);
Token error_token(const char *message);

#endif
