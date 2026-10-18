#ifndef KILN_PARSER_H
#define KILN_PARSER_H
#include "ir/H/ir_lex.h"
#include "codegen/H/emit/emit.h"

// Shared parser state, read by codegen/C/expressions/expr.c as well as parser.c --
// mirrors ashvm/src/compiler/state.h's split (state lives in a small
// header, expression codegen and statement parsing both consume it).
extern Token current;
extern Token previous;
extern CodeBuf *code; // the buffer currently being emitted into

void advance_token(void);
void expect(TokenType type, const char *message);
void parse_error(const char *message) __attribute__((noreturn));

// Compiles the internal IR unit produced by hearth/front (jdriver) and emits machine code for it
// into `out`, single-pass -- parsing and codegen happen together, no separate AST.
void compile_program(const char *source, CodeBuf *out);

#endif
