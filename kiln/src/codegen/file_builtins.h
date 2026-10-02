#ifndef KILN_FILE_BUILTINS_H
#define KILN_FILE_BUILTINS_H

// read_file/file_exists assume their one PATH argument already pushed --
// matching sqrt/abs/floor's convention. write_file/append_file self-parse
// both arguments, matching map/filter's convention (comma-separated).
void codegen_builtin_read_file(void);
void codegen_builtin_write_file(void);
void codegen_builtin_append_file(void);
void codegen_builtin_file_exists(void);

#endif
