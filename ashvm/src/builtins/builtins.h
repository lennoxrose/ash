#ifndef ASH_VM_BUILTINS_H
#define ASH_VM_BUILTINS_H
#include "value/value.h"

int vm_builtin_lookup(const char *name, int len);
VMValue vm_call_builtin(int id, VMValue *args, int argc);

#endif
