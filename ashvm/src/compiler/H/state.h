#ifndef ASH_VM_COMPILER_STATE_H
#define ASH_VM_COMPILER_STATE_H
#include "compiler/H/compiler.h"
#include "lexer/H/lexer.h"

// Shared parser/emitter state for compiler_expr.c and compiler_stmt.c --
// not part of the public API (compiler.h). Mirrors the same
// declare-storage-once, share-via-header pattern ashc used to use for its
// own parser state (see rundown.md).

extern Token current;
extern Token previous;
extern Chunk *chunk;
extern char local_names[MAX_VM_LOCALS][64];
extern int local_count;
extern int lambda_counter;

void advance_token(void);
void expect(TokenType type, const char *message);
int resolve_local(const char *name, int len);
int declare_local(const char *name, int len);
int find_function(const char *name, int len);

// Anchors the opcode's source position on `previous`, mirroring ashc's own
// error-reporting convention (see diagnostics_report call sites).
void emit(uint8_t byte);
void emit_op(uint8_t op);
void emit2(uint8_t a, uint8_t b);
void emit_constant(VMValue v);
int emit_jump(uint8_t op);
void patch_jump(int offset);
void emit_loop(int loop_start);

void set_import_namespace(const char *ns, int len);
void get_import_namespace(const char **out_ns, int *out_len);

// Declares a NEW vm_functions[] entry under "ns.NAME" if a namespace is
// currently set, else under the bare NAME. Returns the new entry's index
// (mirrors compile_fn_decl's own prior inline logic, now centralized so
// imports.c-equivalent code and compile_fn_decl share it). Exits via
// diagnostics_report + exit(1) on table-full or name-too-long, matching
// every other error path in this file.
int declare_function_in_context(const char *name, int len);

// While a namespace is set: resolves ONLY "ns.NAME" (no bare fallback).
// While no namespace is set: resolves NAME directly (identical to
// find_function's existing behavior). Returns -1 if not found.
int find_function_in_context(const char *name, int len);

#endif
