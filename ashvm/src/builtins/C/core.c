#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "builtins/H/internal.h"
#include "vm/H/vm.h"
#include "value/H/hashmap.h"

VMValue vm_call_builtin_core(int id, VMValue *args, int argc) {
    switch (id) {
        case 0: { // len
            if (argc != 1) { vm_runtime_error("len() expects 1 argument\n"); }
            if (args[0].type == VM_STR) return vm_num((int64_t)strlen(args[0].str));
            if (args[0].type == VM_ARRAY) return vm_num(args[0].array->count);
            if (args[0].type == VM_MAP) return vm_num(args[0].map->count);
            vm_runtime_error("len() expects a string, array, or map\n");
        }
        case 1: { // push
            if (argc != 2 || args[0].type != VM_ARRAY) { vm_runtime_error("push(array, value) expected\n"); }
            vm_array_push(args[0].array, args[1]);
            return args[0];
        }
        case 2: { // input
            char buf[1024];
            if (!fgets(buf, sizeof(buf), stdin)) buf[0] = '\0';
            size_t l = strlen(buf);
            if (l > 0 && buf[l - 1] == '\n') buf[l - 1] = '\0';
            char *copy = malloc(strlen(buf) + 1); strcpy(copy, buf);
            return vm_str(copy);
        }
        case 3: { // str
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("str() expects a number\n"); }
            char buf[64];
            if (args[0].number == (long long)args[0].number) snprintf(buf, sizeof(buf), "%lld", (long long)args[0].number);
            else snprintf(buf, sizeof(buf), "%g", args[0].number);
            char *copy = malloc(strlen(buf) + 1); strcpy(copy, buf);
            return vm_str(copy);
        }
        case 4: { // num
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("num() expects a string\n"); }
            return vm_num(strtod(args[0].str, NULL));
        }
        case 5: { // sqrt
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("sqrt() expects a number\n"); }
            return vm_num(sqrt(args[0].number));
        }
        case 6: { // abs
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("abs() expects a number\n"); }
            return vm_num(fabs(args[0].number));
        }
        case 7: { // floor
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("floor() expects a number\n"); }
            return vm_num(floor(args[0].number));
        }
    }
    vm_runtime_error("unknown builtin id: %d\n", id);
}
