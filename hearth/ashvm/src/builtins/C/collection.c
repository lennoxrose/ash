#include <stdlib.h>
#include <string.h>
#include "builtins/H/internal.h"
#include "vm/H/vm.h"
#include "value/H/hashmap.h"

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
                if ((keep.type == VM_NUM || keep.type == VM_BOOL) && keep.number != 0) vm_array_push(result, args[0].array->items[i]);
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
            for (int i = 0; i < args[0].map->size; i++) {
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
            for (int i = 0; i < args[0].map->size; i++) {
                if (args[0].map->entries[i].used) vm_array_push(result, args[0].map->entries[i].value);
            }
            return vm_array_val(result);
        }
        case 13: { // has
            if (argc != 2 || args[0].type != VM_MAP || args[1].type != VM_STR) { vm_runtime_error("has(map, key) expected\n"); }
            return vm_bool(vm_map_has(args[0].map, args[1].str));
        }
        case 14: { // delete -- a map key, or an array index (later items shift left)
            if (argc == 2 && args[0].type == VM_ARRAY && args[1].type == VM_NUM) {
                VMArray *a = args[0].array;
                int i = (int)args[1].number;
                if (i < 0 || i >= a->count) { vm_runtime_error("index out of bounds"); }
                memmove(&a->items[i], &a->items[i + 1], sizeof(VMValue) * (size_t)(a->count - i - 1));
                a->count--;
                return args[0];
            }
            if (argc != 2 || args[0].type != VM_MAP || args[1].type != VM_STR) { vm_runtime_error("delete(map, key) or delete(array, index) expected\n"); }
            vm_map_delete(args[0].map, args[1].str);
            return args[0];
        }
        case 30: { // pop -- removes and returns the last item
            if (argc != 1 || args[0].type != VM_ARRAY) { vm_runtime_error("pop(array) expected\n"); }
            if (args[0].array->count == 0) { vm_runtime_error("pop() on an empty array\n"); }
            return args[0].array->items[--args[0].array->count];
        }
        case 31: { // insert -- insert(array, index, value); index may equal len
            if (argc != 3 || args[0].type != VM_ARRAY || args[1].type != VM_NUM) { vm_runtime_error("insert(array, index, value) expected\n"); }
            VMArray *a = args[0].array;
            int i = (int)args[1].number;
            if (i < 0 || i > a->count) { vm_runtime_error("index out of bounds"); }
            vm_array_push(a, args[2]); // grows; the new slot is then shifted into place
            memmove(&a->items[i + 1], &a->items[i], sizeof(VMValue) * (size_t)(a->count - 1 - i));
            a->items[i] = args[2];
            return args[0];
        }
        case 32: { // slice -- slice(array, from, to) is a new array of items [from, to)
            if (argc != 3 || args[0].type != VM_ARRAY || args[1].type != VM_NUM || args[2].type != VM_NUM) { vm_runtime_error("slice(array, from, to) expected\n"); }
            int from = (int)args[1].number, to = (int)args[2].number;
            if (from < 0 || to > args[0].array->count || from > to) { vm_runtime_error("slice bounds out of range"); }
            VMArray *result = vm_array_new();
            for (int i = from; i < to; i++) vm_array_push(result, args[0].array->items[i]);
            return vm_array_val(result);
        }
        case 33: { // sort -- in place, stable merge sort; sort(array) or sort(array, cmp)
            if ((argc != 1 && argc != 2) || args[0].type != VM_ARRAY) { vm_runtime_error("sort(array) or sort(array, cmp) expected\n"); }
            int has_cmp = (argc == 2);
            if (has_cmp && args[1].type != VM_FUNCTION && args[1].type != VM_CLOSURE) { vm_runtime_error("sort(array, cmp) expected\n"); }
            VMArray *a = args[0].array;
            int n = a->count;
            if (n < 2) return args[0];
            VMValue *src = a->items;
            VMValue *tmp = malloc(sizeof(VMValue) * (size_t)n);
            for (int width = 1; width < n; width *= 2) {
                for (int lo = 0; lo < n; lo += 2 * width) {
                    int mid = lo + width < n ? lo + width : n;
                    int hi = lo + 2 * width < n ? lo + 2 * width : n;
                    int l = lo, r = mid, o = lo;
                    while (l < mid && r < hi) {
                        // take the left item unless it must come after the right one (keeps it stable)
                        int right_first;
                        if (has_cmp) {
                            VMValue call_args[2] = { src[l], src[r] };
                            VMValue res = vm_call_callable_sync(args[1], call_args, 2);
                            if (res.type != VM_NUM) { free(tmp); vm_runtime_error("sort() comparator must return a number\n"); }
                            right_first = res.number > 0;
                        } else if (src[l].type == VM_NUM && src[r].type == VM_NUM) {
                            right_first = src[l].number > src[r].number;
                        } else if (src[l].type == VM_STR && src[r].type == VM_STR) {
                            right_first = strcmp(src[l].str, src[r].str) > 0;
                        } else { free(tmp); vm_runtime_error("sort() cannot compare these values\n"); }
                        tmp[o++] = right_first ? src[r++] : src[l++];
                    }
                    while (l < mid) tmp[o++] = src[l++];
                    while (r < hi) tmp[o++] = src[r++];
                }
                VMValue *swap = src; src = tmp; tmp = swap;
            }
            if (src != a->items) memcpy(a->items, src, sizeof(VMValue) * (size_t)n);
            free(src == a->items ? tmp : src);
            return args[0];
        }
    }
    vm_runtime_error("unknown builtin id: %d\n", id);
}
