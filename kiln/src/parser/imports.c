#define _XOPEN_SOURCE 700 // exposes realpath() and PATH_MAX under -std=c11
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "parser/imports.h"
#include "parser/import_paths.h"
#include "parser/parser.h"
#include "parser/parser_internal.h"
#include "parser/functions.h"
#include "lexer/lexer.h"
#include "diagnostics/diagnostics.h"

#define MAX_KILN_IMPORTS 16
#define MAX_KILN_IMPORT_DEPTH 8

typedef struct {
    char canonical_path[PATH_MAX];
    char namespace_name[64];
} ImportedFile;

static ImportedFile imported_files[MAX_KILN_IMPORTS];
static int imported_file_count = 0;

static char resolving_stack[MAX_KILN_IMPORT_DEPTH][PATH_MAX];
static int resolving_depth = 0;

static char project_root_dir[PATH_MAX];
static char current_file_dir[PATH_MAX];

static int import_block_open = 1;

void imports_init(const char *entry_file_path) {
    char resolved[PATH_MAX];
    if (realpath(entry_file_path, resolved) == NULL) parse_error("could not resolve entry file path");
    import_path_dirname(resolved, project_root_dir, sizeof(project_root_dir));
    size_t root_len = strlen(project_root_dir);
    if (root_len >= sizeof(current_file_dir)) root_len = sizeof(current_file_dir) - 1;
    memcpy(current_file_dir, project_root_dir, root_len);
    current_file_dir[root_len] = '\0';
    import_block_open = 1;
}

void imports_close_block(void) {
    import_block_open = 0;
}

static ImportedFile *registry_find_by_path(const char *canonical) {
    for (int i = 0; i < imported_file_count; i++) {
        if (strcmp(imported_files[i].canonical_path, canonical) == 0) return &imported_files[i];
    }
    return NULL;
}

static ImportedFile *registry_find_by_namespace(const char *ns) {
    for (int i = 0; i < imported_file_count; i++) {
        if (strcmp(imported_files[i].namespace_name, ns) == 0) return &imported_files[i];
    }
    return NULL;
}

static int resolving_stack_contains(const char *canonical) {
    for (int i = 0; i < resolving_depth; i++) {
        if (strcmp(resolving_stack[i], canonical) == 0) return 1;
    }
    return 0;
}

static void report_circular_import(const char *canonical) {
    char msg[1024];
    int off = snprintf(msg, sizeof(msg), "circular import detected: ");
    for (int i = 0; i < resolving_depth; i++) {
        off += snprintf(msg + off, sizeof(msg) - (size_t)off, "%s -> ", resolving_stack[i]);
        if (off >= (int)sizeof(msg)) break;
    }
    snprintf(msg + off, sizeof(msg) - (size_t)off, "%s", canonical);
    parse_error(msg);
}

static char *read_import_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) parse_error("could not open resolved import file");
    fseek(file, 0L, SEEK_END);
    long size = ftell(file);
    rewind(file);
    // Never freed -- an imported file's source buffer must outlive every
    // later use site of its constants (see constants.c), which can be
    // arbitrarily far ahead in the importing program. The compiler is a
    // one-shot host process; leaking these until it exits is the same
    // documented, intentional tradeoff kiln's own runtime heap allocator
    // already makes (see missing.md's GC entry).
    char *buffer = malloc((size_t)size + 1);
    fread(buffer, 1, (size_t)size, file);
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

// The restricted top-level loop for a recursively-parsed imported file:
// identical shape to compile_program's own `while (... ) statement();`
// loop. statement() itself (parser.c) enforces the "only fn/let allowed"
// restriction by checking get_import_namespace()'s length -- see Task 4
// Step 6.
static void parse_imported_file_body(void) {
    // Each imported file gets its OWN fresh import-block window: OPEN
    // (not closed), so the imported file's own leading @import
    // statements (if any -- e.g. a diamond-shaped or chained import) are
    // allowed, exactly like a top-level file's. imports_close_block()
    // would do the opposite (close it), which is wrong here -- statement()
    // itself closes the window the moment it dispatches this file's own
    // first non-import statement, same as it does for the entry file.
    import_block_open = 1;
    while (current.type != TOKEN_EOF) statement();
}

void import_statement(void) {
    if (!import_block_open) parse_error("@import statements must form a contiguous block at the top of the file");
    advance_token(); // consume TOKEN_IMPORT ("@import")

    if (current.type != TOKEN_LESS) parse_error("expected '<' after '@import'");
    Token path = lexer_scan_import_path();
    if (path.type == TOKEN_ERROR) parse_error(path.start);
    advance_token(); // current now holds whatever follows '>' (e.g. ';')

    char canonical[PATH_MAX];
    char message[256];
    if (!import_resolve_path(path.start, path.length, current_file_dir, project_root_dir,
                              canonical, message, sizeof(message))) {
        parse_error(message);
    }

    expect(TOKEN_SEMICOLON, "expected ';' after import path");

    // Cycle check MUST come before the dedup check: a file already in
    // imported_files but STILL on resolving_stack is mid-parse (its
    // registry entry was added before recursing, same as every entry
    // here) -- that's a genuine cycle, not a finished diamond import.
    // Checking the registry first would silently treat a still-parsing
    // file as "already done" and swallow the cycle instead of reporting
    // it. Only a registry hit with NO matching resolving_stack entry is
    // a real, already-completed diamond dedup.
    if (resolving_stack_contains(canonical)) report_circular_import(canonical);

    ImportedFile *existing = registry_find_by_path(canonical);
    if (existing != NULL) return; // diamond dedup -- already fully processed, nothing more to do

    char ns[64];
    import_derive_namespace(canonical, ns, sizeof(ns));

    ImportedFile *collision = registry_find_by_namespace(ns);
    if (collision != NULL) {
        // Sized to comfortably fit two PATH_MAX-bounded paths plus the
        // namespace and surrounding literal text with no truncation, so
        // GCC's -Wformat-truncation can prove this snprintf never overflows.
        char msg[2 * PATH_MAX + 128];
        snprintf(msg, sizeof(msg), "namespace '%s' already used by %s (colliding with %s)",
                  ns, collision->canonical_path, canonical);
        parse_error(msg);
    }

    if (imported_file_count >= MAX_KILN_IMPORTS) parse_error("too many imported files");
    ImportedFile *entry = &imported_files[imported_file_count++];
    size_t canonical_len = strlen(canonical);
    if (canonical_len >= sizeof(entry->canonical_path)) canonical_len = sizeof(entry->canonical_path) - 1;
    memcpy(entry->canonical_path, canonical, canonical_len);
    entry->canonical_path[canonical_len] = '\0';
    size_t ns_len = strlen(ns);
    if (ns_len >= sizeof(entry->namespace_name)) ns_len = sizeof(entry->namespace_name) - 1;
    memcpy(entry->namespace_name, ns, ns_len);
    entry->namespace_name[ns_len] = '\0';

    if (resolving_depth >= MAX_KILN_IMPORT_DEPTH) parse_error("import nesting too deep");
    size_t stack_len = strlen(canonical);
    if (stack_len >= sizeof(resolving_stack[0])) stack_len = sizeof(resolving_stack[0]) - 1;
    memcpy(resolving_stack[resolving_depth], canonical, stack_len);
    resolving_stack[resolving_depth][stack_len] = '\0';
    resolving_depth++;

    // --- Save everything the recursive parse of the imported file will
    // clobber, exactly mirroring vars_save/for_depth_save/loop_depth_save's
    // existing save-recurse-restore pattern (the C call stack gives
    // correct nesting for free, same as those). ---
    DiagnosticsSource saved_diag = diagnostics_save_source();
    LexerState saved_lexer = lexer_save_state();
    Token saved_current = current;
    Token saved_previous = previous;
    char saved_file_dir[PATH_MAX];
    size_t saved_len = strlen(current_file_dir);
    if (saved_len >= sizeof(saved_file_dir)) saved_len = sizeof(saved_file_dir) - 1;
    memcpy(saved_file_dir, current_file_dir, saved_len);
    saved_file_dir[saved_len] = '\0';
    const char *saved_ns; int saved_ns_len;
    get_import_namespace(&saved_ns, &saved_ns_len);
    int saved_import_block_open = import_block_open;

    char *imported_source = read_import_file(canonical);
    diagnostics_set_source(imported_source, canonical);
    import_path_dirname(canonical, current_file_dir, sizeof(current_file_dir));
    set_import_namespace(entry->namespace_name, (int)strlen(entry->namespace_name));

    lexer_init(imported_source);
    advance_token();
    parse_imported_file_body();

    // --- Restore, in the reverse order, so the importing file's parse
    // resumes exactly as if the @import line had been a no-op cursor-wise. ---
    set_import_namespace(saved_ns, saved_ns_len);
    size_t restore_len = strlen(saved_file_dir);
    if (restore_len >= sizeof(current_file_dir)) restore_len = sizeof(current_file_dir) - 1;
    memcpy(current_file_dir, saved_file_dir, restore_len);
    current_file_dir[restore_len] = '\0';
    lexer_restore_state(saved_lexer);
    diagnostics_restore_source(saved_diag);
    current = saved_current;
    previous = saved_previous;
    import_block_open = saved_import_block_open;

    resolving_depth--;
}
