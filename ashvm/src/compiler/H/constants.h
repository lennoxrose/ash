#ifndef ASH_VM_COMPILER_CONSTANTS_H
#define ASH_VM_COMPILER_CONSTANTS_H
#include "value/H/value.h"

#define MAX_VM_CONSTS 32

typedef struct {
    char name[96];
    VMValue value; // built once at declare time via vm_num/vm_str, reused
                     // (by re-emitting, not by sharing the VMValue struct
                     // itself -- see codegen_constant_value) at every use site
} VMConstant;

VMConstant *declare_constant(const char *name, int len, VMValue value);
VMConstant *resolve_constant(const char *name, int len);

// Emits the same OP_CONST codegen a literal of this constant's value
// would have -- used at every `ns.CONST` use site.
void codegen_constant_value(VMConstant *k);

// Called at the top of let_statement(), right after the variable name and
// '=' have been consumed: if an import namespace is currently active,
// consumes the RHS (must be a literal number or string), registers it as
// a namespaced constant, and consumes the trailing ';'. Returns 1 if it
// handled the whole statement (caller should return immediately), 0 if no
// namespace is active (caller should fall through to normal
// variable-declaration handling).
int try_declare_import_constant(const char *name, int len);

#endif
