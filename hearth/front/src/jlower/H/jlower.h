#ifndef JLOWER_H
#define JLOWER_H

#include "jtypes/H/jtypes.h"

/* Lowering of a checked new-Ash program to old-Ash source for the existing ashvm
 * (MIGRATION.md Phase 4). The result is an *imported module* (top-level `local`s of an
 * imported file are visible to its functions) plus a two-line driver.
 *
 * Ownership: all returned strings live in the front end's arena and are never freed.
 * Errors: constructs the backend cannot express yet are reported through cerr(); the
 * caller must check j_error_count() before using the output. */

/* Set to 1 before lowering for Java's 32-bit int wrap-around on + - * << unary- ++ --. Off by default:
 * it roughly halves the speed of integer-heavy loops, and the old VM's numbers are doubles. */
extern int jl_wrap_int;

/* Lowers every loaded class; `entry` supplies the class holding main. Returns the module text. */
char *jlower_program(JClass *entry);

/* The two-line driver that imports the module file `module_name` and starts it. */
char *jlower_driver(const char *module_name);

#endif
