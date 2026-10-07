#ifndef KILN_IMPORTS_H
#define KILN_IMPORTS_H

// Called once, at the very start of compile_program, before the main
// parse loop -- establishes the entry file's own directory as the
// project root that library imports (ash.libs/) resolve against for the
// entire compile, no matter how deeply nested a later import is.
void imports_init(const char *entry_file_path);

// Parses one `@import <path>;` statement. Assumes statement()'s
// dispatcher has NOT yet consumed the TOKEN_IMPORT (this function does
// that itself, matching let_statement()'s own convention).
void import_statement(void);

// Called by statement() on every non-import statement dispatch. No-op
// after the first call in a given file's top-level parse -- marks that
// file's "imports must be a contiguous block at the top" window as
// closed, so a later @import in the SAME file is a compile error. Reset
// automatically on entry to each recursively-parsed imported file (their
// own import block starts fresh).
void imports_close_block(void);

#endif
