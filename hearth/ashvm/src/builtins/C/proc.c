#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "builtins/H/internal.h"
#include "vm/H/vm.h"

// Process builtins for the Ash OS userland; same contract as kiln's
// (kiln/src/codegen/C/builtins/proc_builtins.c). Linux/POSIX only.
VMValue vm_call_builtin_proc(int id, VMValue *args, int argc) {
    switch (id) {
        case 44: { // sys_fork -- pid, 0 in the child
            if (argc != 0) { vm_runtime_error("sys_fork() expects no arguments\n"); }
            fflush(stdout); // buffered output must not be written by both processes
            pid_t pid = fork();
            return vm_num(pid < 0 ? -errno : pid);
        }
        case 45: { // sys_wait -- the waited pid, or negative errno
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("sys_wait(pid) expected\n"); }
            pid_t pid = waitpid((pid_t)args[0].number, NULL, 0);
            return vm_num(pid < 0 ? -errno : pid);
        }
        case 46: { // sys_exec -- only returns on failure (negative errno)
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("sys_exec(path, arg) expected\n"); }
            fflush(stdout);
            char *argv[3] = { args[0].str, args[1].str, NULL };
            if (args[1].str[0] == '\0') argv[1] = NULL; // "" means argv = [path]
            char *envp[1] = { NULL };
            execve(args[0].str, argv, envp);
            return vm_num(-errno);
        }
    }
    vm_runtime_error("unknown builtin id: %d\n", id);
}
