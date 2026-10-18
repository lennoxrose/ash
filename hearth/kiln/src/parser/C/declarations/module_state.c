#include <stdio.h>
#include <string.h>
#include "parser/H/declarations/module_state.h"
#include "parser/H/declarations/functions.h"
#include "parser/H/declarations/constants.h"
#include "parser/H/core/parser.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/layout.h"
#include "codegen/H/emit/runtime_layout.h"

static char names[KILN_MODSTATE_SLOTS][96];
static int count = 0;

int module_state_resolve_qualified(const char *name, int len) {
    for (int i = 0; i < count; i++) {
        if ((int)strlen(names[i]) == len && strncmp(names[i], name, (size_t)len) == 0) return i;
    }
    return -1;
}

int module_state_resolve(const char *name, int len) {
    const char *ns; int ns_len;
    get_import_namespace(&ns, &ns_len);
    if (ns_len == 0) return -1;
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "%.*s.%.*s", ns_len, ns, len, name);
    if (n < 0 || n >= (int)sizeof(buf)) return -1;
    return module_state_resolve_qualified(buf, n);
}

int module_state_declare(const char *name, int len) {
    const char *ns; int ns_len;
    get_import_namespace(&ns, &ns_len);
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "%.*s.%.*s", ns_len, ns, len, name);
    if (n < 0 || n >= (int)sizeof(names[0])) parse_error("module-level name too long (including namespace prefix)");
    if (module_state_resolve_qualified(buf, n) != -1 || resolve_constant(buf, n) != NULL) parse_error("module-level name already declared");
    if (count >= KILN_MODSTATE_SLOTS) parse_error("too many module-level variables");
    memcpy(names[count], buf, (size_t)n);
    names[count][n] = '\0';
    return count++;
}

void module_state_emit_load(int index) {
    emit_mov_reg_imm64(code, REG_RSI, kiln_modstate_addr() + 16u * (unsigned)index);
    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 0);
    emit_push_reg(code, REG_RBX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);
    emit_push_reg(code, REG_RAX);
}

void module_state_emit_store(int index) {
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    emit_mov_reg_imm64(code, REG_RSI, kiln_modstate_addr() + 16u * (unsigned)index);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);
}
