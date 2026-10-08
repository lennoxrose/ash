#include <stdio.h>
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/builtins/builtins.h"
#include "codegen/H/functions/closures.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/functions.h"
#include "parser/H/declarations/vars.h"

// Call codegen -- its own file since it's used from two places
// (expr.c's primary(), for calls used as expressions, and parser.c's
// bare `f(...);` statement) and is a clean, self-contained unit on its
// own, mirroring ashvm's own compiler/calls.c split.

// The '(' is already consumed; current sits on the first argument or ')'.
void codegen_call(Token id) {
    if (codegen_try_builtin_call(id.start, id.length)) return;
    // Milestone 8: a name that resolves to a VARIABLE (holding a
    // TAG_FUNCTION closure/lambda value) goes through the indirect-call
    // path instead of the fast compile-time-known-target path below --
    // purely additive, every existing direct call to a declared `fn` is
    // untouched.
    int slot = resolve_var(id.start, id.length);
    if (slot != -1) { codegen_indirect_call(slot); return; }
    KilnFunction *fn = resolve_function_in_context(id.start, id.length);
    if (fn == NULL) {
        char msg[128];
        snprintf(msg, sizeof(msg), "undefined function: %.*s", id.length, id.start);
        parse_error(msg);
    }
    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        codegen_expression(); argc++;
        while (current.type == TOKEN_COMMA) { advance_token(); codegen_expression(); argc++; }
    }
    expect(TOKEN_RPAREN, "expected ')' after arguments");
    if (argc != fn->arity) {
        char msg[128];
        snprintf(msg, sizeof(msg), "function %.*s expected %d args, got %d", id.length, id.start, fn->arity, argc);
        parse_error(msg);
    }
    if (fn->defined) emit_call_back(code, fn->code_offset);
    else function_add_call_fixup(fn, emit_call_rel32(code));
    if (argc > 0) emit_add_reg_imm32(code, REG_RSP, 16 * argc); // caller cleans up pushed args (each now 16 bytes)
    emit_push_reg(code, REG_RBX); // return convention: RBX=tag, RAX=payload
    emit_push_reg(code, REG_RAX);
}

// codegen_call's body with the name-resolution swapped for a raw (name,
// len) pair instead of a source Token, since a namespaced call site's
// combined name ("math.add") isn't a contiguous span in the original
// source text.
void codegen_call_named(const char *name, int len) {
    KilnFunction *fn = resolve_function(name, len); // already fully-qualified -- no _in_context wrapping
    if (fn == NULL) {
        char msg[128];
        snprintf(msg, sizeof(msg), "undefined function: %.*s", len, name);
        parse_error(msg);
    }
    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        codegen_expression(); argc++;
        while (current.type == TOKEN_COMMA) { advance_token(); codegen_expression(); argc++; }
    }
    expect(TOKEN_RPAREN, "expected ')' after arguments");
    if (argc != fn->arity) {
        char msg[128];
        snprintf(msg, sizeof(msg), "function %.*s expected %d args, got %d", len, name, fn->arity, argc);
        parse_error(msg);
    }
    if (fn->defined) emit_call_back(code, fn->code_offset);
    else function_add_call_fixup(fn, emit_call_rel32(code));
    if (argc > 0) emit_add_reg_imm32(code, REG_RSP, 16 * argc);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
