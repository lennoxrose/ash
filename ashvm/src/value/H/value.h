#ifndef ASH_VM_VALUE_H
#define ASH_VM_VALUE_H
#include <stdint.h>

typedef enum { VM_NUM, VM_STR, VM_ARRAY, VM_MAP, VM_FUNCTION, VM_CLOSURE, VM_NIL, VM_BOOL } VMType;

typedef struct VMArray {
    struct VMValue *items;
    int count;
    int capacity;
} VMArray;

typedef struct VMMap VMMap;

typedef struct VMClosureObj {
    int function_index;
    int capture_count;
    struct VMValue *captured;
} VMClosureObj;

typedef struct VMValue {
    VMType type;
    double number;
    char *str;
    VMArray *array;
    VMMap *map;
    VMClosureObj *closure;
} VMValue;

VMValue vm_num(double n);

// Number -> text in Ash's single format (see value.c).
void vm_format_number(double x, char *buf, size_t size);
VMValue vm_nil(void);
VMValue vm_bool(int b); // yes / no: number holds 1 / 0
int vm_values_equal(VMValue a, VMValue b); // == semantics: by value for strings, numbers, booleans, none; by identity otherwise
VMValue vm_str(char *s);
VMValue vm_str_n(char *s, size_t len); // len = strlen(s), already known
VMValue vm_array_val(VMArray *a);
VMValue vm_map_val(VMMap *m);
VMValue vm_function_val(int function_index);
VMValue vm_closure_val(VMClosureObj *c);

char *vm_copy_string_escaped(const char *start, int length);
VMValue vm_concat_strings(VMValue a, VMValue b); // both VM_STR

VMArray *vm_array_new(void);
void vm_array_push(VMArray *arr, VMValue v);

void vm_print_value(VMValue v);

#endif
