#ifndef ASH_VM_REPL_H
#define ASH_VM_REPL_H

// Interactive session for Java-like Ash. Each accepted line is replayed through the shared front
// end (hearth/front/src/jrepl) and executed in a child process, so a failing line cannot damage
// the session. Returns the process exit status.
int repl_run(void);

// Compiles the internal IR text from hearth/front and runs it on the VM. Exits on a compile error.
void ashvm_execute_ir(char *ir);

#endif
