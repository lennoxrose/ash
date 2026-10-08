#ifndef KILN_CODEGEN_EXPR_H
#define KILN_CODEGEN_EXPR_H
#include "lexer/H/lexer.h"

// Parses one expression and emits real x86-64 code for it via the
// shared `code` buffer (see parser/H/core/parser.h) -- consumes tokens
// directly, no AST. Leaves the computed value's (tag, payload) pushed on
// the (simulated, real CPU) stack.
void codegen_expression(void);

// Parses a call's arguments and emits it -- `id` is the already-consumed
// function/builtin name token, and the '(' must already be consumed too
// (current sits on the first argument or ')'). Leaves the call's (tag,
// payload) result pushed, same as any other expression. Exposed (not
// static to expr.c) so parser.c can also compile a bare `f(...);`
// statement -- calls aren't only usable inside larger expressions.
void codegen_call(Token id);

// codegen_call's counterpart for a namespaced `ns.func(...)` call site,
// where the combined name ("math.add") lives in a local buffer rather
// than a contiguous span of the source text, so there's no single Token
// to hand codegen_call. Already-fully-qualified name -- resolved via
// resolve_function (not resolve_function_in_context), since the caller
// (codegen/C/expressions/expr.c's primary()) has already done the namespace lookup by
// building the combined name.
void codegen_call_named(const char *name, int len);

// Assumes a_tag=RBX, a_payload=RAX, b_tag=RDX, b_payload=RCX already
// popped (matching the pop order every binary-operator codegen in this
// project uses) -- pushes the tag-aware sum (string concat if both are
// STRING, numeric add otherwise). Factored out of additive()'s `+`
// branch so parser.c's `x += e` compound-assignment codegen can reuse
// the exact same logic instead of a second, drifting copy of it.
void codegen_apply_plus(void);

// Pops a value and sets flags for a condition: COND_E afterward means falsy.
// Errors on strings, arrays, maps and functions.
void codegen_pop_and_test_truthy(void);

#endif
