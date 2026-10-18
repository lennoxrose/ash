#include <stdio.h>

#include "jcommon/H/jcommon.h"
#include "jdriver/H/jdriver.h"
#include "jlower/H/jlower.h"
#include "jown/H/own.h"
#include "jparse/H/jparse.h"
#include "jtypes/H/jtypes.h"

static JClass *load_and_check(const char *path, const char *src) {
    Node *head = jparse_unit(path, src);
    jt_init_default_roots(path, head->name);
    JClass *entry = jt_load_unit(path, src);
    jt_check_all();
    if (!j_error_count()) jown_check_project(path);
    if (j_error_count()) {
        fprintf(stderr, "%d error(s)\n", j_error_count());
        return NULL;
    }
    return entry;
}

char *jdriver_compile_source(const char *path, const char *src, int wrap_int) {
    jl_wrap_int = wrap_int;
    JClass *entry = load_and_check(path, src);
    if (!entry) return NULL;
    char *ir = jlower_program(entry);
    if (!ir || j_error_count()) {
        fprintf(stderr, "%d error(s)\n", j_error_count());
        return NULL;
    }
    return ir;
}

char *jdriver_compile_file(const char *path, int wrap_int) {
    char *src = j_read_file(path);
    if (!src) {
        fprintf(stderr, "error: cannot read %s\n", path);
        return NULL;
    }
    return jdriver_compile_source(path, src, wrap_int);
}

int jdriver_check_file(const char *path) {
    char *src = j_read_file(path);
    if (!src) {
        fprintf(stderr, "error: cannot read %s\n", path);
        return 1;
    }
    return load_and_check(path, src) ? 0 : 1;
}
