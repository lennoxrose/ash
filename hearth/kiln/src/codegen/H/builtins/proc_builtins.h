#ifndef KILN_PROC_BUILTINS_H
#define KILN_PROC_BUILTINS_H

// sys_fork() -> pid (0 in the child). Linux only.
// sys_wait(pid) -> wait4 result (pid, or negative errno).
// sys_exec(path, arg) -> only returns on failure (negative errno).
//   argv = [path, arg]; arg == "" means argv = [path]. envp is empty.
// RAX = int64 -> pushes it as a NUMBER (shared with raw_builtins.c).
void codegen_push_rax_as_number(void);
void codegen_builtin_sys_fork(void);
void codegen_builtin_sys_wait(void);
void codegen_builtin_sys_exec(void);

#endif