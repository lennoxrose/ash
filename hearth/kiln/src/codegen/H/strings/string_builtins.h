#ifndef KILN_STRING_BUILTINS_H
#define KILN_STRING_BUILTINS_H

// upper/lower/trim assume their one STRING argument already pushed --
// codegen/C/builtins/builtins.c's dispatch calls these after parsing the argument.
void codegen_builtin_upper(void);
void codegen_builtin_lower(void);
void codegen_builtin_trim(void);

// chr(n) / ord(s): byte <-> one-byte string (argument already pushed).
void codegen_builtin_chr(void);
void codegen_builtin_ord(void);

// substring/indexOf/split/join/replace self-parse their (multiple)
// arguments starting right after the already-consumed '(', same
// convention as codegen/H/builtins/higher_order.h's map/filter/reduce.
void codegen_builtin_substring(void);
void codegen_builtin_indexof(void);
void codegen_builtin_split(void);
void codegen_builtin_join(void);
void codegen_builtin_replace(void);

// contains/starts_with/ends_with (-> 1/0) and repeat(s, n); self-parse like
// indexOf. Implemented in C/strings/affix_builtins.c.
void codegen_builtin_contains(void);
void codegen_builtin_starts_with(void);
void codegen_builtin_ends_with(void);
void codegen_builtin_repeat(void);

#endif
