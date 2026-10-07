#ifndef KILN_FUNCTIONS_H
#define KILN_FUNCTIONS_H

// Flat name -> {code offset, arity} table, mirroring vars.h's pattern (and
// ashvm's vm_functions[]/find_function). Functions can only call ones
// already declared earlier in the source -- same single-pass limitation
// ashvm's compiler has (see emit_call_back's comment) -- so by the time a
// call site compiles, the callee's code_offset is always already known.
//
// Imports (see parser/C/imports/imports.c) don't change this table's shape at all
// -- an imported function is declared under a namespaced string (e.g.
// "math.add") in the exact same flat array. 64 (up from 32) gives real
// programs room to import a handful of libraries without hitting the cap
// immediately; still a hard ceiling, noted as a tuning knob in the design
// spec, not solved generally here.
#define MAX_KILN_FUNCS 64
#define MAX_KILN_PARAMS 8

typedef struct {
    char name[64];
    int code_offset;
    int arity;
} KilnFunction;

// Returns a pointer to the function's entry, or NULL if not declared.
KilnFunction *resolve_function(const char *name, int len);

// Declares a new function (code_offset filled in by the caller once known)
// and returns its entry. Calls parse_error() (noreturn) if the table's full,
// the name is a duplicate, or the name (including any namespace prefix)
// doesn't fit in the 64-byte name field.
KilnFunction *declare_function(const char *name, int len);

// --- Import namespace context (parser/C/imports/imports.c sets/restores this around
// each recursively-parsed imported file; parser_control.c's forge_statement
// and codegen/C/expressions/expr.c + expr_call.c's call-site resolution consult it) ---

// Sets the namespace prefix every subsequent declare_function_in_context/
// resolve_function_in_context call should use. Pass len=0 (ns may be NULL)
// to clear it back to "not inside an import" -- the default/top-level-file
// state.
void set_import_namespace(const char *ns, int len);

// Reads the current namespace prefix (out_len==0 means "none").
void get_import_namespace(const char **out_ns, int *out_len);

// Declares NAME under "ns.NAME" if a namespace is currently set, else
// under the bare NAME -- the single call site forge_statement() uses,
// replacing a direct declare_function() call.
KilnFunction *declare_function_in_context(const char *name, int len);

// While a namespace is set: resolves ONLY "ns.NAME" (no bare fallback --
// an imported file's own bare calls must stay self-contained to its own
// declarations, never accidentally reach into the importing program).
// While no namespace is set: resolves NAME directly, identical to today.
KilnFunction *resolve_function_in_context(const char *name, int len);

#endif
