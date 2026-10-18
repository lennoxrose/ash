#ifndef KILN_CLOSURES_INTERNAL_H
#define KILN_CLOSURES_INTERNAL_H

// Cross-file link between closures.c (object creation, named-function
// values, indirect calls) and lambda.c (lambda literal parsing/codegen) --
// not part of the public API (closures.h).
void closures_alloc_object(int capture_count);
void closures_push_value(void);

#endif
