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

/* Bit mask set by engines that can free memory: lowering then emits `mem_free(x)` for the locals the
 * ownership pass (jown, `memory-safety: on`) proved to be sole owners, of the kinds enabled here.
 * ashvm frees all three; kiln frees Strings only (its allocator can only take those back). */
#define JL_DROP_STRINGS 1
#define JL_DROP_ARRAYS 2
#define JL_DROP_OBJECTS 4
#define JL_DROP_MAPS 16      /* HashMap / HashSet locals and fields (ashvm only: its map entries are chains it knows how to free) */
#define JL_DROP_ELEMS 32     /* free the fresh Strings inside a String[] / List<String> that only it references (ashvm only) */
#define JL_NATIVE_NEWARR 8   /* kiln builds `new T[n]` with the mem_newarr1 builtin (exact capacity) instead of the push loop */
extern int jl_drops;

/* Lowers every loaded class; `entry` supplies the class holding main. Returns the module text. */
char *jlower_program(JClass *entry);

#endif
