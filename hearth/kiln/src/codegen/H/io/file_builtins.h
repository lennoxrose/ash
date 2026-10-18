#ifndef KILN_FILE_BUILTINS_H
#define KILN_FILE_BUILTINS_H

// read_file/file_exists assume their one PATH argument already pushed --
// matching sqrt/abs/floor's convention. write_file/append_file self-parse
// both arguments, matching map/filter's convention (comma-separated).
void codegen_builtin_read_file(void);
void codegen_builtin_write_file(void);
void codegen_builtin_append_file(void);
void codegen_builtin_file_exists(void);

// rename_file(from, to) / delete_file(path) / make_dir(path): 1 on success, 0 on failure.
// list_dir(path): array of entry names. Implemented in C/io/fs_builtins.c.
void codegen_builtin_rename_file(void);
void codegen_builtin_delete_file(void);
void codegen_builtin_make_dir(void);
void codegen_builtin_list_dir(void);

#endif
