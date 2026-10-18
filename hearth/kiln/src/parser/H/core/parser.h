#ifndef KILN_PARSER_H
#define KILN_PARSER_H
#include "lexer/H/lexer.h"
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

// Parses the whole program (milestone 1: a sequence of `print <expr>;`
// statements) and emits machine code for it into `out`, single-pass --
// parsing and codegen happen together, same shape ashvm's compiler uses,
// no separate AST.
void compile_program(const char *source, const char *source_path, CodeBuf *out);

#endif
