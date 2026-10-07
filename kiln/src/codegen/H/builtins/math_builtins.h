#ifndef KILN_MATH_BUILTINS_H
#define KILN_MATH_BUILTINS_H

// Each assumes its one NUMBER argument already pushed -- codegen/C/builtins/builtins.c's
// dispatch calls these after parsing sqrt(x)/abs(x)/floor(x)'s argument.
void codegen_builtin_sqrt(void);
void codegen_builtin_abs(void);
void codegen_builtin_floor(void);

#endif
