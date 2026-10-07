#ifndef ASH_VM_COMPILER_INTERNAL_H
#define ASH_VM_COMPILER_INTERNAL_H
#include "compiler/H/state.h"
#include "compiler/H/constants.h"

// Cross-file forward declarations between compiler_expr.c and
// compiler_stmt.c -- not part of the public API (compiler.h).

// compiler_expr.c
void expression(void);

// compiler_calls.c
void emit_call(Token id);
void emit_call_named(const char *name, int len);
void lambda_literal(void);
// Tries to compile `identifier CMP (number|identifier)` directly into a
// fused OP_CMP_JUMP, used by if/while conditions. Returns the bytecode
// offset to patch_jump() once the branch target is known, or -1 if the
// condition didn't match the fusable shape (caller falls back to a plain
// expression() + OP_JUMP_IF_FALSE).
int try_fuse_condition(void);

// compiler_stmt.c
void block(void);
// statement() itself (compiler_stmt.c's dispatcher) also needs to be
// callable from compiler_stmt_import.c's parse_imported_file_body(),
// which drives the same top-level "while not EOF, parse a statement"
// loop for a recursively parsed imported file -- so it's declared here
// rather than staying static to compiler_stmt.c.
void statement(void);

// compiler_stmt_control.c -- the four statement kinds involved enough to
// warrant their own function (each assumes its keyword token has already
// been consumed by statement()'s dispatch in compiler_stmt.c)
void compile_fn_decl(void);
void compile_if_stmt(void);
void compile_try_stmt(void);
void compile_while_stmt(void);

// compiler_stmt_import.c
void compile_import_stmt(void);
void imports_close_block(void);
void imports_init(const char *entry_file_path);

#endif
