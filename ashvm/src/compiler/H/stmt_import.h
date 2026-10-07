#ifndef ASH_VM_COMPILER_STMT_IMPORT_H
#define ASH_VM_COMPILER_STMT_IMPORT_H

void imports_init(const char *entry_file_path);
void compile_import_stmt(void); // assumes statement() already consumed TOKEN_IMPORT
void imports_close_block(void);

#endif
