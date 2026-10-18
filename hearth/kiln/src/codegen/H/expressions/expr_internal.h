#ifndef KILN_EXPR_INTERNAL_H
#define KILN_EXPR_INTERNAL_H

// Cross-file link between expr.c (arithmetic: primary -> unary -> term ->
// additive) and expr_bool.c (comparison -> logical_and -> logical_or ->
// codegen_expression), not part of the public API (codegen/H/expressions/expr.h).
void additive(void);

#endif
