#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "value/H/value.h"
#include "value/H/hashmap.h"

VMValue vm_num(double n) {
    VMValue v; v.type = VM_NUM; v.number = n; v.str = NULL; v.array = NULL; v.map = NULL; v.closure = NULL; return v;
}
VMValue vm_bool(int b) {
    VMValue v; v.type = VM_BOOL; v.number = b ? 1 : 0; v.str = NULL; v.array = NULL; v.map = NULL; v.closure = NULL; return v;
}
int vm_values_equal(VMValue a, VMValue b) {
    if (a.type != b.type) return 0;
    switch (a.type) {
        case VM_STR: return strcmp(a.str, b.str) == 0;
        case VM_NUM: case VM_BOOL: case VM_FUNCTION: return a.number == b.number;
        case VM_NIL: return 1;
        case VM_ARRAY: return a.array == b.array;
        case VM_MAP: return a.map == b.map;
        case VM_CLOSURE: return a.closure == b.closure;
    }
    return 0;
}
VMValue vm_nil(void) {
    VMValue v; v.type = VM_NIL; v.number = 0; v.str = NULL; v.array = NULL; v.map = NULL; v.closure = NULL; return v;
}
// A string value caches its byte length in `number`, so len(), substring() and
// s[i] are O(1) instead of an strlen each time. vm_str measures once;
// vm_str_n is for callers that already know the length.
VMValue vm_str_n(char *s, size_t len) {
    VMValue v; v.type = VM_STR; v.number = (double)len; v.str = s;
    v.array = NULL; v.map = NULL; v.closure = NULL; return v;
}
VMValue vm_str(char *s) {
    VMValue v; v.type = VM_STR; v.number = (double)strlen(s); v.str = s; v.array = NULL; v.map = NULL; v.closure = NULL; return v;
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

VMValue vm_concat_strings(VMValue a, VMValue b) {
    size_t la = (size_t)a.number, lb = (size_t)b.number;
    char *buf = malloc(la + lb + 1);
    memcpy(buf, a.str, la);
    memcpy(buf + la, b.str, lb);
    buf[la + lb] = '\0';
    return vm_str_n(buf, la + lb);
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

// Drops trailing zeros (and a bare '.') from the fraction in the mantissa
// ending at `end` (exclusive); returns the new end.
static char *trim_fraction(char *start, char *end) {
    if (!memchr(start, '.', (size_t)(end - start))) return end;
    while (end > start && end[-1] == '0') end--;
    if (end > start && end[-1] == '.') end--;
    return end;
}

// Ash's one number-to-text format (kiln's number_format_emit matches it):
// nan/inf/-inf; plain decimal with at most 6 fraction digits (trailing zeros
// dropped) for 0 and 1e-6 <= |x| < 1e15; otherwise scientific with a mantissa
// in [1,10) ("1e+21", "1.5e-07").
void vm_format_number(double x, char *buf, size_t size) {
    if (isnan(x)) { snprintf(buf, size, "nan"); return; }
    if (isinf(x)) { snprintf(buf, size, x < 0 ? "-inf" : "inf"); return; }
    double a = fabs(x);
    if (a == 0 || (a >= 1e-6 && a < 1e15)) {
        snprintf(buf, size, "%.6f", x);
        char *end = trim_fraction(buf, buf + strlen(buf));
        *end = '\0';
        if (strcmp(buf, "-0") == 0) strcpy(buf, "0");
        return;
    }
    snprintf(buf, size, "%.6e", x);
    char *e = strchr(buf, 'e');
    char exponent[16];
    snprintf(exponent, sizeof(exponent), "%s", e);
    char *end = trim_fraction(buf, e);
    memmove(end, exponent, strlen(exponent) + 1);
}

void vm_print_value(VMValue v) {
    if (v.type == VM_NUM) {
        char buf[64];
        vm_format_number(v.number, buf, sizeof(buf));
        printf("%s", buf);
    } else if (v.type == VM_BOOL) {
        printf("%s", v.number != 0 ? "yes" : "no");
    } else if (v.type == VM_STR) {
        printf("%s", v.str);
    } else if (v.type == VM_NIL) {
        printf("none");
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
        for (int i = 0; i < v.map->size; i++) {
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
