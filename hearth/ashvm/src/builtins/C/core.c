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
            if (args[0].type == VM_STR) return vm_num(args[0].number);
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
            if (argc == 1 && args[0].type == VM_BOOL) {
                char *copy = malloc(4); strcpy(copy, args[0].number != 0 ? "yes" : "no");
                return vm_str(copy);
            }
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("str() expects a number\n"); }
            char buf[64];
            vm_format_number(args[0].number, buf, sizeof(buf));
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
        case 27: { // type -- introspection, needed for anything that has to
                   // branch on a value's own type at runtime (json's
                   // stringify is the motivating case: it can't decide
                   // "format this as an array vs. a map vs. a string"
                   // from pure Ash without some way to ask a value what
                   // it is).
            if (argc != 1) { vm_runtime_error("type() expects 1 argument\n"); }
            const char *name;
            switch (args[0].type) {
                case VM_NUM: name = "number"; break;
                case VM_BOOL: name = "boolean"; break;
                case VM_STR: name = "string"; break;
                case VM_ARRAY: name = "array"; break;
                case VM_MAP: name = "map"; break;
                case VM_FUNCTION: case VM_CLOSURE: name = "function"; break;
                case VM_NIL: name = "none"; break;
                default: name = "unknown"; break;
            }
            char *copy = malloc(strlen(name) + 1); strcpy(copy, name);
            return vm_str(copy);
        }
        case 42: { // exit -- exit(code) ends the program with that status
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("exit(code) expected\n"); }
            fflush(stdout);
            exit((int)args[0].number);
        }
        case 28: { // chr -- byte value to a one-byte string (strings are C strings: no 0)
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("chr() expects a number\n"); }
            double n = args[0].number;
            if (n < 1 || n > 255) { vm_runtime_error("chr() expects a number from 1 to 255\n"); }
            char *s = malloc(2); s[0] = (char)(unsigned char)n; s[1] = '\0';
            return vm_str(s);
        }
        case 29: { // ord -- first byte of a string, 0..255
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("ord() expects a string\n"); }
            if (args[0].str[0] == '\0') { vm_runtime_error("ord() expects a non-empty string\n"); }
            return vm_num((unsigned char)args[0].str[0]);
        }
    }
    vm_runtime_error("unknown builtin id: %d\n", id);
}
