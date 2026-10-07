#ifndef ASH_VM_COMPILER_IMPORT_PATHS_H
#define ASH_VM_COMPILER_IMPORT_PATHS_H
#include <limits.h>

// Resolves the raw text inside `@import <...>` to a canonicalized
// (realpath()'d) absolute filesystem path.
//
// raw_path/raw_len: the text between '<' and '>', exactly as scanned by
//   lexer_scan_import_path (NOT null-terminated -- callers must use len).
// importing_file_dir: directory of the file containing this @import
//   statement (relative imports resolve against this).
// project_root_dir: directory of the entry .ash file passed on the
//   command line (library imports resolve against this, regardless of
//   how deeply nested the importing file is).
// canonical_out: buffer of at least PATH_MAX bytes.
//
// Returns 1 and fills canonical_out on success. Returns 0 on failure
// (message_out is filled with a short human-readable reason, e.g.
// "could not resolve '<path>'" or "library import name must not contain
// '.' or '/'" -- callers pass this straight into diagnostics_report).
int import_resolve_path(const char *raw_path, int raw_len,
                         const char *importing_file_dir,
                         const char *project_root_dir,
                         char *canonical_out,
                         char *message_out, int message_out_size);

// Derives the auto-namespace for a resolved canonical path: the file's
// own basename with the trailing ".ash" stripped. Works uniformly for
// both relative and library imports because library entry files are
// required to be named "<libname>/<libname>.ash" -- the folder name and
// the basename-minus-extension are always identical by construction.
void import_derive_namespace(const char *canonical_path, char *out, int out_size);

// Directory-of-a-path helper (hand-rolled, not libgen.h's dirname(),
// which mutates its input and reuses a static buffer across calls --
// both a bad fit here). Writes "." if path has no '/'.
void import_path_dirname(const char *path, char *out, int out_size);

#endif
