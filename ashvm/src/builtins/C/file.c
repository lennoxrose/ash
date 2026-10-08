#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "builtins/H/internal.h"
#include "vm/H/vm.h"

VMValue vm_call_builtin_file(int id, VMValue *args, int argc) {
    switch (id) {
        case 23: { // read_file
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("read_file(path) expected\n"); }
            FILE *f = fopen(args[0].str, "rb");
            if (!f) { vm_runtime_error("could not open file"); }
            fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
            char *buf = malloc(size + 1); size_t n = fread(buf, 1, size, f); buf[n] = '\0'; fclose(f);
            return vm_str(buf);
        }
        case 24: { // write_file
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("write_file(path, content) expected\n"); }
            FILE *f = fopen(args[0].str, "wb");
            if (!f) { vm_runtime_error("could not open file"); }
            fwrite(args[1].str, 1, strlen(args[1].str), f); fclose(f);
            return vm_bool(1);
        }
        case 25: { // append_file
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("append_file(path, content) expected\n"); }
            FILE *f = fopen(args[0].str, "ab");
            if (!f) { vm_runtime_error("could not open file"); }
            fwrite(args[1].str, 1, strlen(args[1].str), f); fclose(f);
            return vm_bool(1);
        }
        case 26: { // file_exists
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("file_exists(path) expected\n"); }
            FILE *f = fopen(args[0].str, "rb");
            if (f) { fclose(f); return vm_bool(1); }
            return vm_bool(0);
        }
        case 34: { // rename_file(from, to) -> 1 on success, 0 on failure
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("rename_file(from, to) expected\n"); }
            return vm_bool(rename(args[0].str, args[1].str) == 0);
        }
        case 35: { // delete_file(path)
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("delete_file(path) expected\n"); }
            return vm_bool(remove(args[0].str) == 0);
        }
        case 36: { // make_dir(path)
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("make_dir(path) expected\n"); }
            return vm_bool(mkdir(args[0].str, 0755) == 0);
        }
        case 37: { // list_dir(path) -> names, no "." / "..", in directory order
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("list_dir(path) expected\n"); }
            DIR *d = opendir(args[0].str);
            if (!d) { vm_runtime_error("cannot open directory"); }
            VMArray *result = vm_array_new();
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                char *copy = malloc(strlen(e->d_name) + 1); strcpy(copy, e->d_name);
                vm_array_push(result, vm_str(copy));
            }
            closedir(d);
            return vm_array_val(result);
        }
    }
    vm_runtime_error("unknown builtin id: %d\n", id);
}
