#include <stdlib.h>
#include <string.h>
#include "builtins/internal.h"
#include "vm/vm.h"
#include "value/hashmap.h"

VMValue vm_call_builtin_collection(int id, VMValue *args, int argc) {
    switch (id) {
        case 8: { // map
            if (argc != 2 || args[0].type != VM_ARRAY || (args[1].type != VM_FUNCTION && args[1].type != VM_CLOSURE)) { vm_runtime_error("map(array, fn) expected\n"); }
            VMArray *result = vm_array_new();
            for (int i = 0; i < args[0].array->count; i++) {
                VMValue call_args[1] = { args[0].array->items[i] };
                vm_array_push(result, vm_call_callable_sync(args[1], call_args, 1));
            }
            return vm_array_val(result);
        }
        case 9: { // filter
            if (argc != 2 || args[0].type != VM_ARRAY || (args[1].type != VM_FUNCTION && args[1].type != VM_CLOSURE)) { vm_runtime_error("filter(array, fn) expected\n"); }
            VMArray *result = vm_array_new();
            for (int i = 0; i < args[0].array->count; i++) {
                VMValue call_args[1] = { args[0].array->items[i] };
                VMValue keep = vm_call_callable_sync(args[1], call_args, 1);
                if (keep.type == VM_NUM && keep.number != 0) vm_array_push(result, args[0].array->items[i]);
            }
            return vm_array_val(result);
        }
        case 10: { // reduce
            if (argc != 3 || args[0].type != VM_ARRAY || (args[1].type != VM_FUNCTION && args[1].type != VM_CLOSURE)) { vm_runtime_error("reduce(array, fn, initial) expected\n"); }
            VMValue acc = args[2];
            for (int i = 0; i < args[0].array->count; i++) {
                VMValue call_args[2] = { acc, args[0].array->items[i] };
                acc = vm_call_callable_sync(args[1], call_args, 2);
            }
            return acc;
        }
        case 11: { // keys
            if (argc != 1 || args[0].type != VM_MAP) { vm_runtime_error("keys() expects a map\n"); }
            VMArray *result = vm_array_new();
            for (int i = 0; i < args[0].map->capacity; i++) {
                if (args[0].map->entries[i].used) {
                    char *k = args[0].map->entries[i].key;
                    char *copy = malloc(strlen(k) + 1); strcpy(copy, k);
                    vm_array_push(result, vm_str(copy));
                }
            }
            return vm_array_val(result);
        }
        case 12: { // values
            if (argc != 1 || args[0].type != VM_MAP) { vm_runtime_error("values() expects a map\n"); }
            VMArray *result = vm_array_new();
            for (int i = 0; i < args[0].map->capacity; i++) {
                if (args[0].map->entries[i].used) vm_array_push(result, args[0].map->entries[i].value);
            }
            return vm_array_val(result);
        }
        case 13: { // has
            if (argc != 2 || args[0].type != VM_MAP || args[1].type != VM_STR) { vm_runtime_error("has(map, key) expected\n"); }
            return vm_num(vm_map_has(args[0].map, args[1].str) ? 1 : 0);
        }
        case 14: { // delete
            if (argc != 2 || args[0].type != VM_MAP || args[1].type != VM_STR) { vm_runtime_error("delete(map, key) expected\n"); }
            vm_map_delete(args[0].map, args[1].str);
            return args[0];
        }
    }
    vm_runtime_error("unknown builtin id: %d\n", id);
}
