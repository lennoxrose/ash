#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "value/value.h"
#include "value/hashmap.h"

VMValue vm_num(double n) {
    VMValue v; v.type = VM_NUM; v.number = n; v.str = NULL; v.array = NULL; v.map = NULL; v.closure = NULL; return v;
}
VMValue vm_str(char *s) {
    VMValue v; v.type = VM_STR; v.number = 0; v.str = s; v.array = NULL; v.map = NULL; v.closure = NULL; return v;
}
VMValue vm_array_val(VMArray *a) {
    VMValue v; v.type = VM_ARRAY; v.number = 0; v.str = NULL; v.array = a; v.map = NULL; v.closure = NULL; return v;
}
VMValue vm_map_val(VMMap *m) {
    VMValue v; v.type = VM_MAP; v.number = 0; v.str = NULL; v.array = NULL; v.map = m; v.closure = NULL; return v;
}
VMValue vm_function_val(int function_index) {
    VMValue v; v.type = VM_FUNCTION; v.number = function_index; v.str = NULL; v.array = NULL; v.map = NULL; v.closure = NULL; return v;
}
VMValue vm_closure_val(VMClosureObj *c) {
    VMValue v; v.type = VM_CLOSURE; v.number = 0; v.str = NULL; v.array = NULL; v.map = NULL; v.closure = c; return v;
}

char *vm_copy_string_escaped(const char *start, int length) {
    char *buf = malloc(length + 1);
    int out = 0;
    for (int i = 0; i < length; i++) {
        if (start[i] == '\\' && i + 1 < length) {
            char next = start[i + 1];
            char c;
            switch (next) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                default: c = next; break;
            }
            buf[out++] = c;
            i++;
        } else {
            buf[out++] = start[i];
        }
    }
    buf[out] = '\0';
    return buf;
}

char *vm_concat_strings(const char *a, const char *b) {
    int la = (int)strlen(a);
    int lb = (int)strlen(b);
    char *buf = malloc(la + lb + 1);
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb);
    buf[la + lb] = '\0';
    return buf;
}

VMArray *vm_array_new(void) {
    VMArray *arr = malloc(sizeof(VMArray));
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
    return arr;
}

void vm_array_push(VMArray *arr, VMValue v) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;
        arr->items = realloc(arr->items, sizeof(VMValue) * arr->capacity);
    }
    arr->items[arr->count++] = v;
}

void vm_print_value(VMValue v) {
    if (v.type == VM_NUM) {
        if (v.number == (long long)v.number) printf("%lld", (long long)v.number);
        else printf("%g", v.number);
    } else if (v.type == VM_STR) {
        printf("%s", v.str);
    } else if (v.type == VM_FUNCTION) {
        printf("<function>");
    } else if (v.type == VM_CLOSURE) {
        printf("<closure>");
    } else if (v.type == VM_ARRAY) {
        printf("[");
        for (int i = 0; i < v.array->count; i++) {
            VMValue item = v.array->items[i];
            if (item.type == VM_STR) printf("\"%s\"", item.str);
            else vm_print_value(item);
            if (i < v.array->count - 1) printf(", ");
        }
        printf("]");
    } else if (v.type == VM_MAP) {
        printf("{");
        int first = 1;
        for (int i = 0; i < v.map->capacity; i++) {
            if (v.map->entries[i].used) {
                if (!first) printf(", ");
                printf("\"%s\": ", v.map->entries[i].key);
                VMValue val = v.map->entries[i].value;
                if (val.type == VM_STR) printf("\"%s\"", val.str);
                else vm_print_value(val);
                first = 0;
            }
        }
        printf("}");
    }
}
