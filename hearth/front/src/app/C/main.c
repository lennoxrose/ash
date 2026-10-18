#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "jcommon/H/jcommon.h"
#include "jlower/H/jlower.h"
#include "jparse/H/jparse.h"
#include "jtypes/H/jtypes.h"

/* ashfront: developer tool for the new-Ash front end.
 *   ashfront --parse <file.ash>   parse only
 *   ashfront --check <file.ash>   parse and type-check (loads the stdlib lazily)
 *   ashfront --lower <file.ash>   print the generated old-Ash module
 *   ashfront --run   <file.ash>   lower and execute on ashvm (found next to ashfront, or $ASHVM)
 *   ashfront --build <file.ash> -o <out>   lower and compile to a native binary with kiln ($KILN)
 * Any mode that lowers also accepts --wrap-int: Java's 32-bit int overflow (about 2x slower arithmetic loops). */

static int write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

static char *self_dir(const char *argv0) {
    char *d = j_strdup(argv0);
    char *slash = strrchr(d, '/');
    if (slash) *slash = 0;
    else strcpy(d, ".");
    return d;
}

int main(int argc, char **argv) {
    /* --wrap-int may appear anywhere after the mode; strip it before positional parsing */
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--wrap-int")) {
            jl_wrap_int = 1;
            for (int j = i; j < argc - 1; j++) argv[j] = argv[j + 1];
            argc--;
            i--;
        }
    }
    const char *mode = argc > 1 ? argv[1] : "";
    int parse = !strcmp(mode, "--parse"), check = !strcmp(mode, "--check");
    int lower = !strcmp(mode, "--lower"), run = !strcmp(mode, "--run"), build = !strcmp(mode, "--build");
    if (argc < 3 || !(parse || check || lower || run || build) || (build && (argc < 5 || strcmp(argv[3], "-o") != 0))) {
        fprintf(stderr, "usage: %s --parse|--check|--lower|--run <file.ash>\n       %s --build <file.ash> -o <output>\n", argv[0], argv[0]);
        return 2;
    }
    char *src = j_read_file(argv[2]);
    if (!src) {
        fprintf(stderr, "error: cannot read %s\n", argv[2]);
        return 1;
    }
    if (parse) {
        Node *u = jparse_unit(argv[2], src);
        printf("parsed %s: %d types\n", argv[2], u->l2.count);
        return 0;
    }
    Node *head = jparse_unit(argv[2], src);
    jt_init_default_roots(argv[2], head->name);
    JClass *entry = jt_load_unit(argv[2], src);
    jt_check_all();
    if (j_error_count()) {
        fprintf(stderr, "%d error(s)\n", j_error_count());
        return 1;
    }
    if (check) {
        printf("checked %s: ok\n", argv[2]);
        return 0;
    }
    char *module = jlower_program(entry);
    if (!module || j_error_count()) {
        fprintf(stderr, "%d error(s)\n", j_error_count());
        return 1;
    }
    if (lower) {
        fputs(module, stdout);
        return 0;
    }
    char dir[] = "/tmp/ashrun_XXXXXX";
    if (!mkdtemp(dir)) {
        perror("mkdtemp");
        return 1;
    }
    char *mod_path = j_fmt("%s/prog.ash", dir), *run_path = j_fmt("%s/run.ash", dir);
    if (write_file(mod_path, module) || write_file(run_path, jlower_driver("prog.ash"))) {
        fprintf(stderr, "error: cannot write to %s\n", dir);
        return 1;
    }
    if (build) {
        const char *kiln = getenv("KILN");
        char *kiln_path = kiln ? j_strdup(kiln) : j_fmt("%s/kiln", self_dir(argv[0]));
        char *bcmd = j_fmt("'%s' '%s' -o '%s'; rc=$?; rm -rf '%s'; exit $rc", kiln_path, run_path, argv[4], dir);
        return system(bcmd) == 0 ? 0 : 1;
    }
    const char *vm = getenv("ASHVM");
    char *vm_path = vm ? j_strdup(vm) : j_fmt("%s/ashvm", self_dir(argv[0]));
    char *cmd = j_fmt("'%s' '%s'; rc=$?; rm -rf '%s'; exit $rc", vm_path, run_path, dir);
    return system(cmd) == 0 ? 0 : 1;
}
