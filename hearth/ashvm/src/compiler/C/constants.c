#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler/H/constants.h"
#include "compiler/H/internal.h"
#include "diagnostics/H/diagnostics.h"

static VMConstant constants[MAX_VM_CONSTS];
static int constant_count = 0;

VMConstant *declare_constant(const char *name, int len, VMValue value) {
    if (constant_count >= MAX_VM_CONSTS) { diagnostics_report("error", "too many imported constants", current.start, current.length); exit(1); }
    if (len >= (int)sizeof(constants[0].name)) { diagnostics_report("error", "constant name too long (including namespace prefix)", current.start, current.length); exit(1); }
    if (resolve_constant(name, len) != NULL) { diagnostics_report("error", "constant already declared", current.start, current.length); exit(1); }
    VMConstant *k = &constants[constant_count++];
    memcpy(k->name, name, (size_t)len);
    k->name[len] = '\0';
    k->value = value;
    return k;
}

VMConstant *resolve_constant(const char *name, int len) {
    for (int i = 0; i < constant_count; i++) {
        if ((int)strlen(constants[i].name) == len && strncmp(constants[i].name, name, len) == 0) return &constants[i];
    }
    return NULL;
}

void codegen_constant_value(VMConstant *k) {
    emit_constant(k->value);
}

int try_declare_import_constant(const char *name, int len) {
    const char *ns; int ns_len;
    get_import_namespace(&ns, &ns_len);
    if (ns_len == 0) return 0;

    char combined[96];
    int combined_len = snprintf(combined, sizeof(combined), "%.*s.%.*s", ns_len, ns, len, name);
    VMValue value;
    if (current.type == TOKEN_NUMBER) {
        value = vm_num(strtod(current.start, NULL));
        advance_token();
    } else if (current.type == TOKEN_STRING) {
        value = vm_str(vm_copy_string_escaped(current.start, current.length));
        advance_token();
    } else {
        diagnostics_report("error", "imported top-level 'local' must be a literal number or string constant", current.start, current.length);
        exit(1);
    }
    declare_constant(combined, combined_len, value);
    expect(TOKEN_SEMICOLON, "expected ';' after statement");
    return 1;
}
