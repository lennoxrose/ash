#ifndef KILN_CLOSURES_H
#define KILN_CLOSURES_H
#include "parser/functions.h"

// `fn (params) { body }` as an EXPRESSION -- current sits on the 'fn'
// token. Captures (by value, snapshotted at creation time -- see the
// scope-limit note in codegen/value.h) every variable currently visible
// in the enclosing scope, then pushes a TAG_FUNCTION value.
void codegen_lambda_expr(void);

// A bare reference to an already-declared named function used as a value
// (not immediately called) -- wraps it in a zero-capture closure object so
// it flows through the same TAG_FUNCTION/indirect-call machinery as a
// lambda literal.
void codegen_named_function_value(KilnFunction *fn);

// `id` already resolved to variable `slot` holding a TAG_FUNCTION value;
// current sits on the first argument or ')' (the '(' already consumed,
// same convention as codegen/expr_call.c's codegen_call). Parses args,
// calls indirectly through the closure object, and pushes the (tag,
// payload) result.
void codegen_indirect_call(int slot);

// Assumes a closure's (tag, payload) is already pushed, followed by
// `argc` more (tag, payload) argument pairs already pushed on top of it
// (in order) -- e.g. codegen/higher_order.c's map/filter/reduce, which
// build both programmatically rather than from source syntax. Performs
// the indirect call and pushes the (tag, payload) result.
void codegen_call_closure_value(int argc);

#endif
