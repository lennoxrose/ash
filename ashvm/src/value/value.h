#ifndef ASH_VM_VALUE_H
#define ASH_VM_VALUE_H
#include <stdint.h>

typedef enum { VM_NUM, VM_STR, VM_ARRAY, VM_MAP, VM_FUNCTION, VM_CLOSURE } VMType;

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
VMValue vm_str(char *s);
VMValue vm_array_val(VMArray *a);
VMValue vm_map_val(VMMap *m);
VMValue vm_function_val(int function_index);
VMValue vm_closure_val(VMClosureObj *c);

char *vm_copy_string_escaped(const char *start, int length);
char *vm_concat_strings(const char *a, const char *b);

VMArray *vm_array_new(void);
void vm_array_push(VMArray *arr, VMValue v);

void vm_print_value(VMValue v);

#endif
