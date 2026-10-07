#include <string.h>
#include <stdint.h>
#include "parser/constants.h"
#include "parser/parser.h"
#include "codegen/emit.h"
#include "codegen/value.h"
#include "codegen/strings.h"

static KilnConstant constants[MAX_KILN_CONSTS];
static int constant_count = 0;

static KilnConstant *declare_constant_common(const char *name, int len) {
    if (constant_count >= MAX_KILN_CONSTS) parse_error("too many imported constants");
    if (len >= (int)sizeof(constants[0].name)) parse_error("constant name too long (including namespace prefix)");
    if (resolve_constant(name, len) != NULL) parse_error("constant already declared");
    KilnConstant *k = &constants[constant_count++];
    memcpy(k->name, name, (size_t)len);
    k->name[len] = '\0';
    return k;
}

KilnConstant *declare_number_constant(const char *name, int len, double value) {
    KilnConstant *k = declare_constant_common(name, len);
    k->kind = KILN_CONST_NUMBER;
    k->num_value = value;
    return k;
}

KilnConstant *declare_string_constant(const char *name, int len, const char *str_bytes, int str_len) {
    KilnConstant *k = declare_constant_common(name, len);
    k->kind = KILN_CONST_STRING;
    k->str_bytes = (char *)str_bytes;
    k->str_len = str_len;
    return k;
}

KilnConstant *resolve_constant(const char *name, int len) {
    for (int i = 0; i < constant_count; i++) {
        if ((int)strlen(constants[i].name) == len && strncmp(constants[i].name, name, len) == 0) return &constants[i];
    }
    return NULL;
}

void codegen_constant_value(KilnConstant *k) {
    if (k->kind == KILN_CONST_STRING) {
        codegen_string_literal_bytes(k->str_bytes, k->str_len);
        return;
    }
    uint64_t bits;
    memcpy(&bits, &k->num_value, sizeof(bits));
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_mov_reg_imm64(code, REG_RAX, bits);
    emit_push_reg(code, REG_RAX);
}
