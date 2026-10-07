#ifndef KILN_EACH_LOOP_H
#define KILN_EACH_LOOP_H

// each_statement itself is declared in parser_internal.h alongside
// given_statement/during_statement/forge_statement/attempt_statement (the
// other statement-compiler cross-file links) -- this header exists only
// for the depth counter below, which forge_statement/lambda.c also need.
//
// The compile-time each-nesting-depth counter's save/reset/restore, for
// forge_statement/lambda.c to wrap around a called body's compilation
// (same spirit as parser/H/declarations/vars.h's
// vars_save/vars_clear/vars_restore): a function or lambda body gets its
// own RBP at runtime, so its own `each` loops can safely reuse the same
// offsets independently of how deep the enclosing code's nesting was --
// see vars.h's each_level_offset comment.
void each_depth_save(int *out);
void each_depth_reset(void);
void each_depth_restore(int saved);

#endif
