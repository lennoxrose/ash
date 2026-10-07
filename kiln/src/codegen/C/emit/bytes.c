#include "codegen/H/emit/bytes.h"
#include "codegen/H/platform/win_call.h"
#include "elf/H/elf_dynamic_call.h"
#include "app/H/target.h"

// Plan B, phase B1: emitted once (jumped over), called via emit_call_back
// from all 18 call sites instead of each re-emitting this loop inline --
// same pattern as codegen/C/runtime/heap.c's alloc routine. Phase B3/B4: skipped
// entirely under --link=shared (this routine lives in libkilnrt.so/.dll
// instead, emitted by the separate "compile the runtime" driver via
// bytes_emit_copy_routine_only), and bytes_emit_copy becomes a call
// through that library instead.
static int copy_routine_offset = -1;

static void emit_copy_routine(CodeBuf *code) {
    int skip = emit_jmp_rel32(code);
    copy_routine_offset = code->count;
    emit_mov_reg_reg(code, REG_RCX, REG_RDX);
    int loop_start = code->count;
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int done = emit_jcc_rel32(code, COND_E);
    emit_load_byte_reg(code, REG_RAX, REG_RBX);
    emit_store_byte_reg(code, REG_RDI, REG_RAX);
    emit_add_reg_imm8(code, REG_RBX, 1);
    emit_add_reg_imm8(code, REG_RDI, 1);
    emit_dec_reg(code, REG_RCX);
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, done);
    emit_ret(code);
    emit_patch_jump(code, skip);
}

void bytes_emit_copy_routine_only(CodeBuf *code) {
    emit_copy_routine(code);
}

int bytes_copy_routine_offset(void) { return copy_routine_offset; }

static int is_shared(void) {
    return kiln_get_link_mode() == KILN_LINK_SHARED && !kiln_is_compiling_so();
}

void bytes_emit_startup(CodeBuf *code) {
    if (!is_shared()) emit_copy_routine(code);
}

void bytes_emit_copy(CodeBuf *code) {
    if (is_shared()) {
        if (kiln_get_target() == KILN_TARGET_WINDOWS) win_call_runtime_import(code, RUNTIME_IMPORT_BYTES_COPY);
        else elf_dynamic_call(code, RUNTIME_IMPORT_BYTES_COPY);
        return;
    }
    emit_call_back(code, copy_routine_offset);
}
