#ifndef APM_PROC_H
#define APM_PROC_H

#include <stddef.h>

// Runs another program WITHOUT a shell: argv is passed to execvp as is, so nothing in an argument (a package
// name, a URL from a server) can ever be interpreted as shell syntax. Used for curl and git.
//
// `input` (may be NULL) is written to the program's stdin, its stdout is captured into *out. stderr is the
// caller's own. Returns the exit status (0..255; 127 when the program could not be started) or -1 when
// the pipes/fork failed or the output exceeded `max_out` bytes.
// Ownership: *out is malloc'd and NUL-terminated and the caller frees it (it is NULL after -1). `extra_env`
// (may be NULL) is a NULL-terminated list of "KEY=VALUE" strings added to the child's environment.
int proc_run(char *const argv[], const char *input, size_t input_len, char **out, size_t max_out, const char *const *extra_env);

// Starts argv with all three standard streams on /dev/null and waits for it (opening a browser). Returns its exit status or -1.
int proc_quiet(char *const argv[]);

#endif
