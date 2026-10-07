#include <string.h>
#include "builtins/builtins.h"
#include "builtins/internal.h"
#include "vm/vm.h"

static const char *builtin_names[] = {
    "len", "push", "input", "str", "num", "sqrt", "abs", "floor",
    "map", "filter", "reduce", "keys", "values", "has", "delete",
    "split", "join", "substring", "indexOf", "replace", "upper", "lower", "trim",
    "read_file", "write_file", "append_file", "file_exists"
};
#define NUM_BUILTINS (int)(sizeof(builtin_names) / sizeof(builtin_names[0]))

int vm_builtin_lookup(const char *name, int len) {
    for (int i = 0; i < NUM_BUILTINS; i++) {
        if ((int)strlen(builtin_names[i]) == len && strncmp(builtin_names[i], name, len) == 0) return i;
    }
    return -1;
}

VMValue vm_call_builtin(int id, VMValue *args, int argc) {
    if (id >= 0 && id <= 7) return vm_call_builtin_core(id, args, argc);
    if (id >= 8 && id <= 14) return vm_call_builtin_collection(id, args, argc);
    if (id >= 15 && id <= 22) return vm_call_builtin_string(id, args, argc);
    if (id >= 23 && id <= 26) return vm_call_builtin_file(id, args, argc);
    vm_runtime_error("unknown builtin id: %d\n", id);
}
