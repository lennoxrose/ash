#ifndef JOWN_H
#define JOWN_H

/* Ownership checking for projects that turn it on (`memory-safety: on` in ash.pkg).
 *
 * Runs after the type checker, on the typed AST, and only on the sources of the project that owns the
 * entry file (never on the standard library or dependencies, whose code the user cannot change).
 * Rules (goals.md item 1, first milestone):
 *   - A local of an owning type (any class, String or array; not primitives, boxes or enums) is MOVED
 *     when it is stored: `a = b`, `T a = b`, `this.f = b`, `xs[i] = b`, or `{b, ...}`. Using `b`
 *     afterwards is an error until it is assigned a new value.
 *   - Passing a value to a call, constructor or return is a borrow, not a move.
 *   - Control flow is followed: a move in one branch of an `if`/`switch`, or in a loop body that can
 *     run again, counts afterwards; `return`, `throw`, `break` and `continue` end a path.
 * Errors: reported with cerr(), so compilation fails like any other type error.
 * Default when ash.pkg is missing or has no `memory-safety:` key: on (JOWN_DEFAULT_ON); with no ash.pkg
 * at all the entry file's directory is the project. */

#include "jparse/H/jast.h"
#include "jtypes/H/jtypes.h"
#include "jtypes/H/check.h"

#define JOWN_DEFAULT_ON 1

/* Checks the project that contains `entry_path`. Does nothing when memory safety is off. */
void jown_check_project(const char *entry_path);

/* A String built by a non-constant `+`: a new heap value that nothing else references yet. */
int jown_is_fresh_string(Node *e);

/* The String / array instance fields of plain class `c` that its objects exclusively own: nothing else can
 * hold their values, so freeing an object frees them too. Valid after jown_check_project. `*out` is a static buffer. */
int jown_owned_fields(JClass *c, JField ***out);
/* `n` names an owned field through a plain base (`f`, `this.f`, `local.f`): its old value can be freed on assignment. */
int jown_field_assign_frees(Node *n);

/* A freed String[] / List<String> (local or owned field) whose elements are all fresh Strings only it references:
 * the elements are freed with it. */
int jown_local_elems(JLocal *l);
int jown_field_elems(JField *f);

#endif
