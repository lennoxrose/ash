#ifndef KILN_KEYS_VALUES_H
#define KILN_KEYS_VALUES_H

// Both assume their one MAP argument already pushed -- codegen/C/builtins/builtins.c's
// dispatch calls these after parsing keys(m)/values(m)'s argument.
void codegen_builtin_keys(void);
void codegen_builtin_values(void);

#endif
