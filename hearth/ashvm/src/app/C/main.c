#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/H/repl.h"
#include "compiler/H/compiler.h"
#include "vm/H/vm.h"
#include "diagnostics/H/diagnostics.h"
#include "jcommon/H/jcommon.h"
#include "jdriver/H/jdriver.h"
#include "jexplain/H/jexplain.h"
#include "jfmt/H/jfmt.h"
#include "jlower/H/jlower.h"

#ifndef ASH_VERSION
#define ASH_VERSION "dev"
#endif

void ashvm_execute_ir(char *ir) {
    diagnostics_set_source(ir, "<internal ir>");
    vm_run(compile(ir));
}

static int usage(const char *prog) {
    fprintf(stderr, "usage: %s                            interactive REPL\n", prog);
    fprintf(stderr, "       %s run [--wrap-int] <file.ash>   run a program\n", prog);
    fprintf(stderr, "       %s fmt [--check|--write] <file.ash>...   fix indentation and trailing white space (to stdout; --write edits the files, --check lists files that would change)\n", prog);
    fprintf(stderr, "       %s explain [<code>]              describe a memory-safety error code such as E0101 (no code: list them)\n", prog);
    fprintf(stderr, "       %s check [--json] <file.ash>     type-check without running; --json prints the diagnostics as JSON on stdout\n", prog);
    return 2;
}

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "--version") == 0) { printf("ashvm %s\n", ASH_VERSION); return 0; }
    if (argc < 2) return repl_run();

    if (strcmp(argv[1], "run") == 0) {
        int wrap_int = argc == 4 && strcmp(argv[2], "--wrap-int") == 0;
        if (argc != 3 + wrap_int) return usage(argv[0]);
        jl_drops = JL_DROP_STRINGS | JL_DROP_ARRAYS | JL_DROP_OBJECTS | JL_DROP_MAPS | JL_DROP_ELEMS; // ashvm can free: lower mem_free for the locals jown proved to be sole owners
        char *ir = jdriver_compile_file(argv[2 + wrap_int], wrap_int);
        if (!ir) return 1;
        ashvm_execute_ir(ir);
        return 0;
    }
    if (strcmp(argv[1], "fmt") == 0) {
        int check = 0, write = 0, first = 2;
        for (; first < argc && argv[first][0] == '-'; first++) {
            if (!strcmp(argv[first], "--check")) check = 1;
            else if (!strcmp(argv[first], "--write") || !strcmp(argv[first], "-w")) write = 1;
            else return usage(argv[0]);
        }
        if (first >= argc || (check && write) || (!check && !write && argc - first != 1)) return usage(argv[0]);
        int status = 0;
        for (int i = first; i < argc; i++) {
            char *src = j_read_file(argv[i]);
            if (!src) { fprintf(stderr, "error: cannot read %s\n", argv[i]); status = 1; continue; }
            const char *err;
            char *out = jfmt_format(argv[i], src, &err);
            if (!out) { fprintf(stderr, "%s: %s\n", argv[i], err); status = 1; continue; }
            int same = strcmp(src, out) == 0;
            if (check) { if (!same) { printf("%s\n", argv[i]); status = 1; } }
            else if (write) {
                if (!same) {
                    FILE *f = fopen(argv[i], "wb");
                    if (!f || fputs(out, f) == EOF || fclose(f) != 0) { fprintf(stderr, "error: cannot write %s\n", argv[i]); status = 1; }
                }
            } else fputs(out, stdout);
        }
        return status;
    }
    if (strcmp(argv[1], "explain") == 0) {
        if (argc > 3) return usage(argv[0]);
        return jexplain_print(argc == 3 ? argv[2] : NULL);
    }
    if (strcmp(argv[1], "check") == 0) {
        int json = argc == 4 && strcmp(argv[2], "--json") == 0;
        if (argc != 3 + json || argv[2 + json][0] == '-') return usage(argv[0]);
        if (json) j_diag_json();
        return jdriver_check_file(argv[2 + json]);
    }
    return usage(argv[0]);
}
