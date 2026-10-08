#ifndef KILN_EXIT_BUILTIN_H
#define KILN_EXIT_BUILTIN_H

// exit(code): ends the process with that exit status (self-parses its argument,
// starting right after the already-consumed '(').
void codegen_builtin_exit(void);

#endif
