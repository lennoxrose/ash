#ifndef KILN_CODEGEN_PRINT_INT_H
#define KILN_CODEGEN_PRINT_INT_H
#include "codegen/H/emit/emit.h"

// Emitted once, at program start (see parser.c's compile_program) --
// must run before any call to codegen_print_top below.
void print_emit_startup(CodeBuf *code);

// Plan B: emits ONLY the print routine's body -- used by the standalone
// "compile the runtime" driver that builds libkilnrt.so.
void print_emit_top_routine_only(CodeBuf *code);
int print_top_routine_offset(void);

// Pops the top-of-stack expression result (a double, milestone 4) into
// XMM0, converts it to decimal ASCII (handles negative values and a
// fractional part, trimming trailing fraction zeros so whole numbers
// print with no decimal point -- matching ashvm's dual %lld/%g print
// format), and writes it + a trailing newline to stdout via a single raw
// write(2) syscall. No libc, no external symbols. Plan B, phase B1: a
// real CALL to one shared copy (emitted by print_emit_startup) rather
// than re-emitting this whole routine inline at every print statement.
void codegen_print_top(CodeBuf *code);

// mov rdi, 0; mov rax, 60 (exit); syscall -- always the last instruction
// in the generated program.
void codegen_exit0(CodeBuf *code);

#endif
