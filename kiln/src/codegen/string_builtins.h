#ifndef KILN_STRING_BUILTINS_H
#define KILN_STRING_BUILTINS_H

// upper/lower/trim assume their one STRING argument already pushed --
// codegen/builtins.c's dispatch calls these after parsing the argument.
void codegen_builtin_upper(void);
void codegen_builtin_lower(void);
void codegen_builtin_trim(void);

// substring/indexOf/split/join/replace self-parse their (multiple)
// arguments starting right after the already-consumed '(', same
// convention as codegen/higher_order.h's map/filter/reduce.
void codegen_builtin_substring(void);
void codegen_builtin_indexof(void);
void codegen_builtin_split(void);
void codegen_builtin_join(void);
void codegen_builtin_replace(void);

#endif
