#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtins/internal.h"
#include "vm/vm.h"

VMValue vm_call_builtin_file(int id, VMValue *args, int argc) {
    switch (id) {
        case 23: { // read_file
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("read_file(path) expected\n"); }
            FILE *f = fopen(args[0].str, "rb");
            if (!f) { vm_runtime_error("could not open file: %s\n", args[0].str); }
            fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
            char *buf = malloc(size + 1); size_t n = fread(buf, 1, size, f); buf[n] = '\0'; fclose(f);
            return vm_str(buf);
        }
        case 24: { // write_file
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("write_file(path, content) expected\n"); }
            FILE *f = fopen(args[0].str, "wb");
            if (!f) { vm_runtime_error("could not write file: %s\n", args[0].str); }
            fwrite(args[1].str, 1, strlen(args[1].str), f); fclose(f);
            return vm_num(1);
        }
        case 25: { // append_file
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("append_file(path, content) expected\n"); }
            FILE *f = fopen(args[0].str, "ab");
            if (!f) { vm_runtime_error("could not open file for append: %s\n", args[0].str); }
            fwrite(args[1].str, 1, strlen(args[1].str), f); fclose(f);
            return vm_num(1);
        }
        case 26: { // file_exists
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("file_exists(path) expected\n"); }
            FILE *f = fopen(args[0].str, "rb");
            if (f) { fclose(f); return vm_num(1); }
            return vm_num(0);
        }
    }
    vm_runtime_error("unknown builtin id: %d\n", id);
}
