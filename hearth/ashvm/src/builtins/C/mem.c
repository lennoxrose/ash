#include <stdlib.h>
#include "builtins/H/internal.h"
#include "value/H/hashmap.h"
#include "vm/H/vm.h"

// mem_release(x) is System.free(x) written by the program (memory-safety: off); mem_free(x) is the compiler's.
// mem_cat(a, b, flags) concatenates and frees temporary operands; mem_free(x) frees a String or array that the compiler proved to be the sole owner of a fresh heap value
// (front/src/jown, `memory-safety: on` in ash.pkg). Only the value's own buffer is freed; elements of an
// array may be shared and are left alone. Anything else is ignored. Under the experimental collector
// (make GC=1) the collector owns every object, so freeing here would double-free: it does nothing.
VMValue vm_call_builtin_mem(int id, VMValue *args, int argc) {
    if (id == 48) { // mem_cat(a, b, flags): a + b, then free the operands that were temporaries (bit 1 = a, bit 2 = b)
        if (argc != 3 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("mem_cat(a, b, flags) expects strings\n"); }
        VMValue r = vm_concat_strings(args[0], args[1]);
#ifndef ASH_GC
        int fl = (int)args[2].number;
        if (fl & 1) free(args[0].str);
        if (fl & 2) free(args[1].str);
#endif
        return r;
    }
    if (argc != 1) { vm_runtime_error("mem_free(x) expects 1 argument\n"); }
#ifndef ASH_GC
    VMValue v = args[0];
    if (id == 53) {              // mem_free_elems(x): the Strings of a String[] / List<String> that only it references
        if (v.type == VM_ARRAY && v.array) {
            for (int i = 0; i < v.array->count; i++) if (v.array->items[i].type == VM_STR) free(v.array->items[i].str);
        }
        return vm_nil();
    }
    if (v.type == VM_STR) {
        free(v.str);
    } else if (v.type == VM_ARRAY && v.array) {
        free(v.array->items);
        free(v.array);
    } else if (id == 52 && v.type == VM_MAP && v.map) {  // a HashMap / HashSet: the entry chains it built, then the map itself
        VMMap *m = v.map;
        for (int i = 0; i < m->size; i++) {
            if (!m->entries[i].used) continue;
            VMValue cur = m->entries[i].value;           // [key, value, next] entries; the element count is a number
            while (cur.type == VM_ARRAY && cur.array && cur.array->count == 3) {
                VMValue next = cur.array->items[2];
                free(cur.array->items);
                free(cur.array);
                cur = next;
            }
            free(m->entries[i].key);
        }
        free(m->entries);
        free(m->index);
        free(m);
    } else if (v.type == VM_MAP && v.map) {             // an object: its keys, tables and the map; field values are left alone
        VMMap *m = v.map;
        for (int i = 0; i < m->size; i++) if (m->entries[i].used) free(m->entries[i].key);
        free(m->entries);
        free(m->index);
        free(m);
    }
#endif
    return vm_nil();
}
