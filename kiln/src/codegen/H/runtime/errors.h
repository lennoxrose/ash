#ifndef KILN_ERRORS_H
#define KILN_ERRORS_H
#include "codegen/H/emit/emit.h"

// Runtime (not compile-time -- see parser/H/core/parser.h's parse_error for
// that) errors: array out of bounds, missing map key, and so on.
// Emitted once, at the very start of the generated program (right after
// codegen/C/runtime/heap.c's heap_emit_startup): a single reusable "raise" routine
// that every runtime error jumps to (never called -- there's nothing to
// return to, control either resumes at a handle block or the process
// exits).
void errors_emit_startup(CodeBuf *code);

// Plan B: emits ONLY the raise routine's body -- used by the standalone
// "compile the runtime" driver that builds libkilnrt.so.
void errors_emit_raise_routine_only(CodeBuf *code);
int errors_raise_routine_offset(void);

// Embeds `msg` (compile-time-known, jumped over so it's never executed
// as instructions -- same technique codegen/C/strings/strings.c uses for string
// literals) and raises it: if an `attempt` is currently active (see
// parser/C/statements/attempt_handle.c), unwinds to its `handle` block with `msg` bound as
// a TAG_STRING value; otherwise writes it to stderr and exits with
// status 1, same as before milestone 9.
void errors_emit_die(CodeBuf *code, const char *msg);

// Like errors_emit_die, but ALWAYS fatal -- bypasses the attempt/handle check
// entirely. Used only for the one internal case that must never be
// "catchable": exceeding MAX_KILN_TRY_DEPTH itself (silently continuing
// there would mean pushing a handler past the fixed table, corrupting
// the adjacent code segment).
void errors_emit_fatal(CodeBuf *code, const char *msg);

// Like errors_emit_die, but for a RUNTIME string value (parser.c's
// `raise expr;`) instead of a compile-time-constant C string -- the
// caller must already have RSI=message payload and RDX=message length
// loaded (a raise expression's already-popped (tag,payload), payload is
// already a valid length-prefixed TAG_STRING pointer, so there's nothing
// to embed, just the same jump to the raise routine errors_emit_die uses.
void errors_emit_die_dynamic(CodeBuf *code);

#endif
