#ifndef JPARSE_H
#define JPARSE_H

#include "jparse/H/jast.h"

/* Parses one new-Ash source file into an N_UNIT. Syntax errors abort with a diagnostic. */
Node *jparse_unit(const char *path, const char *src);

/* For the language server (jlsp): when `jparse_capture_path` names a file, the tokens of its latest parse are left in
 * `jparse_captured`, so that the identifier tokens (whose text pointer a node keeps as its `name`) can be told apart
 * by position. Not used by compilation. */
#include "jlex/H/jlex.h"
extern const char *jparse_capture_path;
extern JTokens jparse_captured;

#endif
