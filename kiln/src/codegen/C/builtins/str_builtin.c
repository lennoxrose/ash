#include <string.h>
#include "codegen/convert_builtins.h"
#include "codegen/emit.h"
#include "codegen/emit_sse.h"
#include "codegen/value.h"
#include "codegen/heap.h"
#include "codegen/bytes.h"
#include "parser/parser.h"

static void load_double_const(XReg dst, double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    emit_mov_reg_imm64(code, REG_RAX, bits);
    emit_movq_xmm_from_reg(code, dst, REG_RAX);
}

// Scratch layout while building digits, same shape as print_int.c:
//   [rsp, rsp+32)   integer digits (+ optional '-'), built backward
//   rsp+32          '.' (only if there's a fraction)
//   [rsp+33, rsp+39) up to 6 fraction digits, built forward
void codegen_builtin_str(void) {
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag (assumed NUMBER)
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_sub_reg_imm8(code, REG_RSP, 40);

    // ---- sign ----
    emit_pxor_xmm_xmm(code, XMM2);
    emit_ucomisd(code, XMM0, XMM2);
    emit_mov_reg_imm64(code, REG_RBX, 0);
    int skip_negate = emit_jcc_rel32(code, COND_AE);
    emit_pxor_xmm_xmm(code, XMM2);
    emit_subsd(code, XMM2, XMM0);
    emit_movsd_xmm_xmm(code, XMM0, XMM2);
    emit_mov_reg_imm64(code, REG_RBX, 1);
    emit_patch_jump(code, skip_negate);

    // ---- split into integer part (RAX) and fractional remainder (XMM0) ----
    emit_cvttsd2si(code, REG_RAX, XMM0);
    emit_cvtsi2sd(code, XMM1, REG_RAX);
    emit_subsd(code, XMM0, XMM1);

    // ---- integer digits, backward ----
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_add_reg_imm8(code, REG_RCX, 32);
    int loop_start = code->count;
    emit_dec_reg(code, REG_RCX);
    emit_cqo(code);
    emit_mov_reg_imm64(code, REG_RSI, 10);
    emit_idiv_reg(code, REG_RSI);
    emit_add_reg_imm8(code, REG_RDX, '0');
    emit_store_byte_reg(code, REG_RCX, REG_RDX);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    emit_jcc_back(code, COND_NE, loop_start);

    // ---- sign prepend ----
    emit_cmp_reg_imm32(code, REG_RBX, 0);
    int skip_sign = emit_jcc_rel32(code, COND_E);
    emit_dec_reg(code, REG_RCX);
    emit_store_byte_imm(code, REG_RCX, '-');
    emit_patch_jump(code, skip_sign);

    // ---- fraction digits, forward, round-half-up first (see
    // print_int.c's comment for why) ----
    load_double_const(XMM1, 0.0000005);
    emit_addsd(code, XMM0, XMM1);
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_add_reg_imm8(code, REG_RSI, 33);
    load_double_const(XMM3, 10.0);
    emit_mov_reg_imm64(code, REG_RBX, 0); // frac_len
    for (int i = 0; i < 6; i++) {
        emit_mulsd(code, XMM0, XMM3);
        emit_cvttsd2si(code, REG_RAX, XMM0);
        emit_cvtsi2sd(code, XMM1, REG_RAX);
        emit_subsd(code, XMM0, XMM1);
        emit_cmp_reg_imm32(code, REG_RAX, 0);
        int skip_mark = emit_jcc_rel32(code, COND_E);
        emit_mov_reg_imm64(code, REG_RBX, (uint64_t)(i + 1));
        emit_patch_jump(code, skip_mark);
        emit_add_reg_imm8(code, REG_RAX, '0');
        emit_store_byte_reg(code, REG_RSI, REG_RAX);
        emit_add_reg_imm8(code, REG_RSI, 1);
    }

    // ---- decide '.' placement, compute the content's end pointer (no
    // trailing newline -- this is a string value, not a print) ----
    emit_mov_reg_reg(code, REG_RDX, REG_RSP);
    emit_add_reg_imm8(code, REG_RDX, 32);
    emit_cmp_reg_imm32(code, REG_RBX, 0);
    int whole_number = emit_jcc_rel32(code, COND_E);
    emit_store_byte_imm(code, REG_RDX, '.');
    emit_mov_reg_reg(code, REG_RDX, REG_RSP);
    emit_add_reg_imm8(code, REG_RDX, 33);
    emit_add_reg_reg(code, REG_RDX, REG_RBX);
    int after_dot = emit_jmp_rel32(code);
    emit_patch_jump(code, whole_number);
    emit_patch_jump(code, after_dot);

    emit_sub_reg_reg(code, REG_RDX, REG_RCX); // RDX = content length

    // ---- heap-allocate a length-prefixed block and copy the digits in ----
    emit_mov_reg_reg(code, REG_RDI, REG_RDX);
    emit_add_reg_imm8(code, REG_RDI, 8);
    heap_emit_alloc(code); // RAX = new block, clobbers RSI

    emit_store_mem_disp32(code, REG_RAX, 0, REG_RDX); // length prefix
    emit_mov_reg_reg(code, REG_RSI, REG_RAX);          // stash block addr (bytes_emit_copy preserves RSI)
    emit_mov_reg_reg(code, REG_RDI, REG_RSI);
    emit_add_reg_imm8(code, REG_RDI, 8);               // dest = block+8
    emit_mov_reg_reg(code, REG_RBX, REG_RCX);          // src = digit buffer start
    bytes_emit_copy(code); // clobbers RAX,RBX,RCX,RDX,RDI; leaves RSI = block addr

    emit_add_reg_imm8(code, REG_RSP, 40); // release the digit scratch

    emit_mov_reg_reg(code, REG_RAX, REG_RSI);
    emit_add_reg_imm8(code, REG_RAX, 8); // payload = block+8

    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
