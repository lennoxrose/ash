#ifndef KILN_FOR_LOOP_H
#define KILN_FOR_LOOP_H

// for_statement itself is declared in parser_internal.h alongside
// if_statement/while_statement/fn_statement/try_statement (the other
// statement-compiler cross-file links) -- this header exists only for
// the depth counter below, which fn_statement/lambda.c also need.
//
// The compile-time for-nesting-depth counter's save/reset/restore, for
// fn_statement/lambda.c to wrap around a called body's compilation (same
// spirit as parser/H/declarations/vars.h's vars_save/vars_clear/vars_restore): a
// function or lambda body gets its own RBP at runtime, so its own `for`
// loops can safely reuse the same offsets independently of how deep the
// enclosing code's nesting was -- see vars.h's for_level_offset comment.
void for_depth_save(int *out);
void for_depth_reset(void);
void for_depth_restore(int saved);

#endif
