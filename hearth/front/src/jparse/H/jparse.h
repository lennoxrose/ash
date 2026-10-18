#ifndef JPARSE_H
#define JPARSE_H

#include "jparse/H/jast.h"

/* Parses one new-Ash source file into an N_UNIT. Syntax errors abort with a diagnostic. */
Node *jparse_unit(const char *path, const char *src);

#endif
