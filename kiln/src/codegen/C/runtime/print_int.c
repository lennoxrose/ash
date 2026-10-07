#include <string.h>
#include "codegen/H/runtime/print_int.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/emit/layout.h"
#include "codegen/H/platform/win_call.h"
#include "codegen/H/platform/platform_console.h"
#include "elf/H/elf_dynamic_call.h"
#include "app/H/target.h"

// Loads a compile-time-known double literal into an xmm register, via a
// GP register holding its raw IEEE-754 bit pattern -- same technique
// codegen/C/expressions/expr.c uses for user-written number literals (there's no "mov
// xmm, imm64" instruction).
static void load_double_const(CodeBuf *code, XReg dst, double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    emit_mov_reg_imm64(code, REG_RAX, bits);
    emit_movq_xmm_from_reg(code, dst, REG_RAX);
}

// Scratch layout, all within one `sub rsp, 40` region:
//   [rsp, rsp+32)   integer digits (+ optional '-'), built BACKWARD --
//                   same technique milestone 1 used, RCX decrements
//                   before each store so no reversal step is needed.
//   rsp+32          '.' (only meaningful if frac_len > 0)
//   [rsp+33, rsp+39) up to 6 fraction digits, built FORWARD
//   dynamic         '\n', placed right after whichever of the above
//                   actually got used, so the whole thing writes in ONE
//                   contiguous write(2) call regardless of whether this
//                   number turned out to be whole or fractional.
// RBX holds the sign flag through phase 3, then gets reused as frac_len
// (how many of the 6 fraction digits are significant, i.e. up to the
// last nonzero one) -- safe, sign has already been written to the
// buffer by the time phase 4 starts.
// Dispatches on the popped value's tag: a STRING just gets written
// directly (it's already bytes -- no digit conversion needed at all,
// unlike a number); everything below this point is the NUMBER path,
// unchanged in shape from milestone 4 except that it now starts from an
// already-popped payload (in RAX) instead of popping XMM0 itself.
//
// Plan B, phase B1: emitted once (jumped over), called via emit_call_back
// from codegen_print_top below -- this is the single biggest deduplication
// win of B1, since this routine used to be re-emitted in full at EVERY
// print statement in the source (dwarfing heap_emit_alloc/bytes_emit_copy,
// which are individually smaller). Takes its value in RAX=payload,
// RBX=tag (NOT by popping them itself): a real CALL pushes a return
// address on top of whatever the caller already pushed, so a `pop` here
// would grab the return address instead of the caller's value -- same
// register-based-argument reasoning codegen/C/runtime/heap.c's alloc routine
// already uses, not something print specifically needed before this
// became a real subroutine. Each of its three paths (string/nil/number)
// still balances its own RSP usage back to the entry value, which a real
// subroutine does need.
static int print_routine_offset = -1;

static void emit_print_routine(CodeBuf *code) {
    int routine_skip = emit_jmp_rel32(code);
    print_routine_offset = code->count;
    emit_cmp_reg_imm32(code, REG_RBX, TAG_STRING);
    int is_number = emit_jcc_rel32(code, COND_NE);

    // ---- STRING path: [payload-8] = length, [payload..] = bytes ----
    emit_load_mem_disp32(code, REG_RDX, REG_RAX, -8); // length
    emit_mov_reg_reg(code, REG_RSI, REG_RAX);          // buf
    platform_emit_write_stdout(code);
    // newline, as its own tiny write (the string's bytes aren't
    // guaranteed to be followed by one in memory)
    emit_sub_reg_imm8(code, REG_RSP, 8);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_store_byte_imm(code, REG_RCX, '\n');
    emit_mov_reg_reg(code, REG_RSI, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, 1);
    platform_emit_write_stdout(code);
    emit_add_reg_imm8(code, REG_RSP, 8);
    int done = emit_jmp_rel32(code);
    emit_patch_jump(code, is_number);

    // ---- NIL path ----
    emit_cmp_reg_imm32(code, REG_RBX, TAG_NIL);
    int not_nil = emit_jcc_rel32(code, COND_NE);
    {
        int skip = emit_jmp_rel32(code);
        int data_offset = code->count;
        emit_byte(code, 'n'); emit_byte(code, 'i'); emit_byte(code, 'l'); emit_byte(code, '\n');
        emit_patch_jump(code, skip);
        // Position-independent path: needed only for libkilnrt.so, whose
        // load address ld.so chooses at runtime. libkilnrt.dll (Windows)
        // is fixed-base like everything else kiln emits (see
        // pe/H/pe_dll_writer.h), so kiln_code_base() already resolves
        // correctly there without RIP-relative addressing -- it now
        // knows about DLL compilation specifically (see codegen/C/emit/layout.c).
        if (kiln_is_compiling_so() && kiln_get_target() == KILN_TARGET_LINUX) {
            emit_lea_rip_back(code, REG_RSI, data_offset);
        } else {
            uint64_t addr = kiln_code_base() + (uint64_t)data_offset;
            emit_mov_reg_imm64(code, REG_RSI, addr);
        }
        emit_mov_reg_imm64(code, REG_RDX, 4);
        platform_emit_write_stdout(code);
    }
    int done_nil = emit_jmp_rel32(code);
    emit_patch_jump(code, not_nil);

    // ---- NUMBER path ----
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX); // payload bits -> double
    emit_sub_reg_imm8(code, REG_RSP, 40);

    // ---- sign ----
    emit_pxor_xmm_xmm(code, XMM2); // XMM2 = +0.0, used as the comparison zero
    emit_ucomisd(code, XMM0, XMM2);
    emit_mov_reg_imm64(code, REG_RBX, 0);
    int skip_negate = emit_jcc_rel32(code, COND_AE); // XMM0 >= 0 -> not negative
    emit_pxor_xmm_xmm(code, XMM2);
    emit_subsd(code, XMM2, XMM0);    // XMM2 = 0.0 - XMM0 = -XMM0
    emit_movsd_xmm_xmm(code, XMM0, XMM2);
    emit_mov_reg_imm64(code, REG_RBX, 1);
    emit_patch_jump(code, skip_negate);

    // ---- split into integer part (RAX) and fractional remainder (XMM0, now in [0,1)) ----
    emit_cvttsd2si(code, REG_RAX, XMM0);
    emit_cvtsi2sd(code, XMM1, REG_RAX);
    emit_subsd(code, XMM0, XMM1);

    // ---- integer digits, backward, same loop shape as milestone 1 ----
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

    // ---- fraction digits, forward, 6 unrolled steps (a fixed, small,
    // compile-time-known count -- no runtime loop needed) ----
    // Round-half-up at the 6th decimal place before extracting digits:
    // repeated float arithmetic (e.g. adding 0.1 a few times) routinely
    // lands a hair below the "true" decimal value (0.7999999999999999
    // instead of 0.8) due to ordinary double rounding, and truncating
    // that directly would print "0.799999" instead of "0.8". Adding half
    // a unit-in-the-last-place first is the standard fix; it's too small
    // to introduce a spurious digit for values that are already exact
    // (see the loop comment below).
    load_double_const(code, XMM1, 0.0000005);
    emit_addsd(code, XMM0, XMM1);
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_add_reg_imm8(code, REG_RSI, 33);
    load_double_const(code, XMM3, 10.0);
    emit_mov_reg_imm64(code, REG_RBX, 0); // repurposed: frac_len (sign already applied above)
    for (int i = 0; i < 6; i++) {
        emit_mulsd(code, XMM0, XMM3);
        emit_cvttsd2si(code, REG_RAX, XMM0);   // this step's digit, 0-9
        emit_cvtsi2sd(code, XMM1, REG_RAX);
        emit_subsd(code, XMM0, XMM1);          // remainder for the next digit
        emit_cmp_reg_imm32(code, REG_RAX, 0);
        int skip_mark = emit_jcc_rel32(code, COND_E);
        emit_mov_reg_imm64(code, REG_RBX, (uint64_t)(i + 1)); // "kept up through here"
        emit_patch_jump(code, skip_mark);
        emit_add_reg_imm8(code, REG_RAX, '0');
        emit_store_byte_reg(code, REG_RSI, REG_RAX);
        emit_add_reg_imm8(code, REG_RSI, 1);
    }

    // ---- decide '.'/newline placement, then a single write(2) ----
    emit_mov_reg_reg(code, REG_RDX, REG_RSP);
    emit_add_reg_imm8(code, REG_RDX, 32);
    emit_cmp_reg_imm32(code, REG_RBX, 0);
    int whole_number = emit_jcc_rel32(code, COND_E);
    emit_store_byte_imm(code, REG_RDX, '.');
    emit_mov_reg_reg(code, REG_RDX, REG_RSP);
    emit_add_reg_imm8(code, REG_RDX, 33);
    emit_add_reg_reg(code, REG_RDX, REG_RBX); // + frac_len
    int after_dot = emit_jmp_rel32(code);
    emit_patch_jump(code, whole_number);
    // RDX already points at rsp+32, the correct newline spot when there's no fraction
    emit_patch_jump(code, after_dot);
    emit_store_byte_imm(code, REG_RDX, '\n');

    emit_mov_reg_reg(code, REG_RSI, REG_RCX); // buf start
    emit_add_reg_imm8(code, REG_RDX, 1);
    emit_sub_reg_reg(code, REG_RDX, REG_RCX); // length = (newline_pos + 1) - buf_start
    platform_emit_write_stdout(code);

    emit_add_reg_imm8(code, REG_RSP, 40); // restore stack for the next statement
    emit_patch_jump(code, done);
    emit_patch_jump(code, done_nil);
    emit_ret(code);
    emit_patch_jump(code, routine_skip);
}

void print_emit_top_routine_only(CodeBuf *code) {
    emit_print_routine(code);
}

int print_top_routine_offset(void) { return print_routine_offset; }

static int is_shared(void) {
    return kiln_get_link_mode() == KILN_LINK_SHARED && !kiln_is_compiling_so();
}

void print_emit_startup(CodeBuf *code) {
    if (!is_shared()) emit_print_routine(code);
}

void codegen_print_top(CodeBuf *code) {
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    if (is_shared()) {
        if (kiln_get_target() == KILN_TARGET_WINDOWS) win_call_runtime_import(code, RUNTIME_IMPORT_PRINT_TOP);
        else elf_dynamic_call(code, RUNTIME_IMPORT_PRINT_TOP);
        return;
    }
    emit_call_back(code, print_routine_offset);
}

void codegen_exit0(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        win_call_begin(code, 0);
        win_call_arg_imm64(code, 0, 0); // ExitProcess(0)
        win_call_import(code, PE_IMPORT_EXIT_PROCESS);
        return; // never returns -- no win_call_end needed
    }
    emit_mov_reg_imm64(code, REG_RDI, 0);
    emit_mov_reg_imm64(code, REG_RAX, 60); // syscall: exit
    emit_syscall(code);
}
