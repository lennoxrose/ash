#ifndef KILN_TYPE_BUILTIN_H
#define KILN_TYPE_BUILTIN_H

// type(value) -- returns "number"/"string"/"array"/"map"/"function"/"none".
// Needed for anything that has to branch on a value's own type at
// runtime (the motivating case: a JSON stringify can't decide "format
// this as an array vs. a map vs. a string" from pure Ash without some
// way to ask a value what it is -- see ideas/ash_modules.md).
void codegen_builtin_type(void);

#endif
