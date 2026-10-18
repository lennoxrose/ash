#include <string.h>
#include "builtins/H/builtins.h"
#include "builtins/H/internal.h"
#include "vm/H/vm.h"

static const char *builtin_names[] = {
    "len", "push", "input", "str", "num", "sqrt", "abs", "floor",
    "map", "filter", "reduce", "keys", "values", "has", "delete",
    "split", "join", "substring", "indexOf", "replace", "upper", "lower", "trim",
    "read_file", "write_file", "append_file", "file_exists",
    "type", "chr", "ord",
    "pop", "insert", "slice", "sort",
    "rename_file", "delete_file", "make_dir", "list_dir",
    "contains", "starts_with", "ends_with", "repeat",
    "exit", "matrix_mul"
};
// Argument count each builtin is wrapped with when used as a value
// (`map(xs, str)`); same order as builtin_names.
static const int builtin_arity[] = {
    1, 2, 0, 1, 1, 1, 1, 1,
    2, 2, 3, 1, 1, 2, 2,
    2, 2, 3, 2, 3, 1, 1, 1,
    1, 2, 2, 1,
    1, 1, 1,
    1, 3, 3, 1,
    2, 1, 1, 1,
    2, 2, 2, 2,
    1, 2
};
#define NUM_BUILTINS (int)(sizeof(builtin_names) / sizeof(builtin_names[0]))

int vm_builtin_lookup(const char *name, int len) {
    for (int i = 0; i < NUM_BUILTINS; i++) {
        if ((int)strlen(builtin_names[i]) == len && strncmp(builtin_names[i], name, len) == 0) return i;
    }
    return -1;
}

int vm_builtin_arity(int id) { return builtin_arity[id]; }

VMValue vm_call_builtin(int id, VMValue *args, int argc) {
    if (id >= 0 && id <= 7) return vm_call_builtin_core(id, args, argc);
    if (id >= 8 && id <= 14) return vm_call_builtin_collection(id, args, argc);
    if (id >= 15 && id <= 22) return vm_call_builtin_string(id, args, argc);
    if (id >= 23 && id <= 26) return vm_call_builtin_file(id, args, argc);
    if (id >= 27 && id <= 29) return vm_call_builtin_core(id, args, argc); // type, chr, ord
    if (id == 42) return vm_call_builtin_core(id, args, argc); // exit
    if (id >= 38 && id <= 41) return vm_call_builtin_string(id, args, argc); // contains, starts_with, ends_with, repeat
    if (id >= 34 && id <= 37) return vm_call_builtin_file(id, args, argc); // rename_file, delete_file, make_dir, list_dir
    if (id >= 30 && id <= 33) return vm_call_builtin_collection(id, args, argc); // pop, insert, slice, sort
    if (id == 43) return vm_call_builtin_matrix(id, args, argc); // matrix_mul
    vm_runtime_error("unknown builtin id: %d\n", id);
}
