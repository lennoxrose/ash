#ifndef ASH_VM_BUILTINS_INTERNAL_H
#define ASH_VM_BUILTINS_INTERNAL_H
#include "value/value.h"

// Each group handles a contiguous id range within vm_call_builtin's switch;
// see builtins.c for the range -> group mapping and the shared name table.
VMValue vm_call_builtin_core(int id, VMValue *args, int argc);
VMValue vm_call_builtin_collection(int id, VMValue *args, int argc);
VMValue vm_call_builtin_string(int id, VMValue *args, int argc);
VMValue vm_call_builtin_file(int id, VMValue *args, int argc);

#endif
