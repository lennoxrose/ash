#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/H/repl.h"
#include "compiler/H/compiler.h"
#include "vm/H/vm.h"
#include "diagnostics/H/diagnostics.h"
#include "jdriver/H/jdriver.h"

void ashvm_execute_ir(char *ir) {
    diagnostics_set_source(ir, "<internal ir>");
    vm_run(compile(ir));
}

static int usage(const char *prog) {
    fprintf(stderr, "usage: %s                            interactive REPL\n", prog);
    fprintf(stderr, "       %s run [--wrap-int] <file.ash>   run a program\n", prog);
    fprintf(stderr, "       %s check <file.ash>              type-check without running\n", prog);
    return 2;
}

int main(int argc, char *argv[]) {
    if (argc < 2) return repl_run();

    if (strcmp(argv[1], "run") == 0) {
        int wrap_int = argc == 4 && strcmp(argv[2], "--wrap-int") == 0;
        if (argc != 3 + wrap_int) return usage(argv[0]);
        char *ir = jdriver_compile_file(argv[2 + wrap_int], wrap_int);
        if (!ir) return 1;
        ashvm_execute_ir(ir);
        return 0;
    }
    if (strcmp(argv[1], "check") == 0) {
        if (argc != 3) return usage(argv[0]);
        return jdriver_check_file(argv[2]);
    }
    return usage(argv[0]);
}
