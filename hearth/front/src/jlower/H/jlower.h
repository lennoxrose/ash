#ifndef JLOWER_H
#define JLOWER_H

#include "jtypes/H/jtypes.h"

/* Lowering of a checked new-Ash program to the internal IR (ir/H/ir_lex.h) that ashvm and kiln consume.
 * The result is one unit of top-level `forge` and `local` declarations; `forge start()` is the program
 * entry, and the consumer calls it. The IR is a compiler-internal representation, not Ash source.
 *
 * Ownership: all returned strings live in the front end's arena and are never freed.
 * Errors: constructs the backend cannot express yet are reported through cerr(); the
 * caller must check j_error_count() before using the output. */

/* Set to 1 before lowering for Java's 32-bit int wrap-around on + - * << unary- ++ --. Off by default:
 * it roughly halves the speed of integer-heavy loops, and the old VM's numbers are doubles. */
extern int jl_wrap_int;

/* Lowers every loaded class; `entry` supplies the class holding main. Returns the module text. */
char *jlower_program(JClass *entry);

#endif
