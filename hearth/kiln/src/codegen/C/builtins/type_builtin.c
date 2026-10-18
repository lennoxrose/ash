#include "codegen/H/builtins/type_builtin.h"
#include "codegen/H/strings/strings.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/value.h"
#include "parser/H/core/parser.h"

// Pops (tag, payload), pushes one of seven string literals depending on
// the tag -- TAG_FUNCTION covers both a plain named function and a
// closure, matching value.h's documented "capture_count=0 is just a
// zero-capture closure" shape (kiln has no separate closure tag the way
// ashvm's VM_CLOSURE vs. VM_FUNCTION does, so there's only one string to
// return either way). The argument itself is already parsed and pushed
// by the caller (codegen_try_builtin_call), same convention every other
// single-argument builtin here uses (upper/lower/trim/str/num/...).
void codegen_builtin_type(void) {
    emit_pop_reg(code, REG_RAX); // payload (unused -- only the tag matters)
    emit_pop_reg(code, REG_RBX); // tag

    int jumps[6];
    int n = 0;

    emit_cmp_reg_imm32(code, REG_RBX, TAG_NUMBER);
    int not_number = emit_jcc_rel32(code, COND_NE);
    codegen_string_literal_bytes("number", 6);
    jumps[n++] = emit_jmp_rel32(code);
    emit_patch_jump(code, not_number);

    emit_cmp_reg_imm32(code, REG_RBX, TAG_BOOL);
    int not_bool = emit_jcc_rel32(code, COND_NE);
    codegen_string_literal_bytes("boolean", 7);
    jumps[n++] = emit_jmp_rel32(code);
    emit_patch_jump(code, not_bool);

    emit_cmp_reg_imm32(code, REG_RBX, TAG_STRING);
    int not_string = emit_jcc_rel32(code, COND_NE);
    codegen_string_literal_bytes("string", 6);
    jumps[n++] = emit_jmp_rel32(code);
    emit_patch_jump(code, not_string);

    emit_cmp_reg_imm32(code, REG_RBX, TAG_ARRAY);
    int not_array = emit_jcc_rel32(code, COND_NE);
    codegen_string_literal_bytes("array", 5);
    jumps[n++] = emit_jmp_rel32(code);
    emit_patch_jump(code, not_array);

    emit_cmp_reg_imm32(code, REG_RBX, TAG_MAP);
    int not_map = emit_jcc_rel32(code, COND_NE);
    codegen_string_literal_bytes("map", 3);
    jumps[n++] = emit_jmp_rel32(code);
    emit_patch_jump(code, not_map);

    emit_cmp_reg_imm32(code, REG_RBX, TAG_FUNCTION);
    int not_function = emit_jcc_rel32(code, COND_NE);
    codegen_string_literal_bytes("function", 8);
    jumps[n++] = emit_jmp_rel32(code);
    emit_patch_jump(code, not_function);

    // TAG_NIL, or anything else -- "none" is the only tag left in
    // practice, so this is both the nil case and the safety net.
    codegen_string_literal_bytes("none", 4);

    for (int i = 0; i < n; i++) emit_patch_jump(code, jumps[i]);
}
