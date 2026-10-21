#include <stddef.h>
#include <stdint.h>
#include "codegen/H/builtins/raw_builtins.h"
#include "codegen/H/builtins/proc_builtins.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/runtime/errors.h"
#include "app/H/target.h"
#include "parser/H/core/parser.h"

// Values are (tag, payload) pairs, payload on top of the stack; a number's payload is a double's bit pattern.

// Pops a NUMBER and leaves it truncated to an int64 in dst. Clobbers RAX, RBX, XMM0.
static void pop_int(Reg dst) {
    emit_pop_reg(code, REG_RAX);
    emit_pop_reg(code, REG_RBX);
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, dst, XMM0);
}

static void push_nil(void) {
    emit_mov_reg_imm64(code, REG_RBX, TAG_NIL);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RBX);
}

// On Windows the arguments are still evaluated, then the program stops with an error.
static int unsupported_target(void) {
    if (kiln_get_target() != KILN_TARGET_WINDOWS) return 0;
    errors_emit_die(code, "runtime error: raw builtins are Linux only");
    return 1;
}

static void clear_hi_regs(void) {
    emit_mov_hi_reg_imm32(code, 10, 0);
    emit_mov_hi_reg_imm32(code, 8, 0);
    emit_mov_hi_reg_imm32(code, 9, 0);
}

void codegen_builtin_raw_syscall(void) {
    for (int i = 0; i < 7; i++) {
        codegen_expression();
        if (i < 6) expect(TOKEN_COMMA, "expected ',' in raw_syscall(n, a1, a2, a3, a4, a5, a6)");
    }
    expect(TOKEN_RPAREN, "expected ')' after arguments");
    if (unsupported_target()) {
        emit_add_reg_imm32(code, REG_RSP, 7 * 16);
        emit_mov_reg_imm64(code, REG_RAX, 0);
        codegen_push_rax_as_number();
        return;
    }
    // argument i's payload is at [rbx + (6 - i) * 16]; convert each straight into its syscall register
    static const struct { int arg; Reg reg; int hi; } load[] = {
        {6, REG_RAX, 9}, {5, REG_RAX, 8}, {4, REG_RAX, 10},
        {3, REG_RDX, 0}, {2, REG_RSI, 0}, {1, REG_RDI, 0},
    };
    emit_mov_reg_reg(code, REG_RBX, REG_RSP);
    for (size_t i = 0; i < sizeof(load) / sizeof(load[0]); i++) {
        emit_load_mem_disp32(code, REG_RAX, REG_RBX, (6 - load[i].arg) * 16);
        emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
        emit_cvttsd2si(code, load[i].reg, XMM0);
        if (load[i].hi) emit_mov_hi_reg_from_rax(code, load[i].hi);
    }
    emit_load_mem_disp32(code, REG_RAX, REG_RBX, 6 * 16); // the syscall number
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RAX, XMM0);
    emit_add_reg_imm32(code, REG_RSP, 7 * 16);
    emit_syscall(code);
    clear_hi_regs();
    codegen_push_rax_as_number();
}

void codegen_builtin_raw_alloc(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    if (unsupported_target()) {
        emit_mov_reg_imm64(code, REG_RAX, 0);
        codegen_push_rax_as_number();
        return;
    }
    pop_int(REG_RSI);
    emit_add_reg_imm32(code, REG_RSI, 16);                // room for the size header munmap needs later
    emit_mov_reg_imm64(code, REG_RDI, 0);
    emit_mov_reg_imm64(code, REG_RDX, 3);                 // PROT_READ | PROT_WRITE
    emit_mov_hi_reg_imm32(code, 10, 0x22);                // MAP_PRIVATE | MAP_ANONYMOUS
    emit_mov_hi_reg_imm32(code, 8, -1);                   // fd
    emit_mov_hi_reg_imm32(code, 9, 0);                    // offset
    emit_mov_reg_imm64(code, REG_RAX, 9);                 // syscall: mmap
    emit_syscall(code);
    clear_hi_regs();
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int ok = emit_jcc_rel32(code, COND_GE);
    emit_mov_reg_imm64(code, REG_RAX, 0);                 // failure: negative errno becomes address 0
    int done = emit_jmp_rel32(code);
    emit_patch_jump(code, ok);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RSI);
    emit_add_reg_imm32(code, REG_RAX, 16);
    emit_patch_jump(code, done);
    codegen_push_rax_as_number();
}

void codegen_builtin_raw_free(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    if (!unsupported_target()) {
        pop_int(REG_RDI);
        emit_cmp_reg_imm32(code, REG_RDI, 0);
        int skip = emit_jcc_rel32(code, COND_E);
        emit_sub_reg_imm32(code, REG_RDI, 16);
        emit_load_mem_disp32(code, REG_RSI, REG_RDI, 0);  // the size header
        emit_mov_reg_imm64(code, REG_RAX, 11);            // syscall: munmap
        emit_syscall(code);
        emit_patch_jump(code, skip);
    } else {
        emit_add_reg_imm32(code, REG_RSP, 16);
    }
    push_nil();
}

// Dispatches on the width in RCX to one of four emitters, a runtime error for anything but 1, 2, 4, 8.
static void width_switch(const char *name, void (*emit)(int width)) {
    int done[4];
    static const int widths[4] = {1, 2, 4, 8};
    for (int i = 0; i < 4; i++) {
        emit_cmp_reg_imm32(code, REG_RCX, widths[i]);
        int next = emit_jcc_rel32(code, COND_NE);
        emit(widths[i]);
        done[i] = emit_jmp_rel32(code);
        emit_patch_jump(code, next);
    }
    errors_emit_die(code, name);
    for (int i = 0; i < 4; i++) emit_patch_jump(code, done[i]);
}

static void peek_width(int width) {
    switch (width) {
        case 1: emit_load_byte_reg(code, REG_RAX, REG_RSI); break;
        case 2: emit_load_word(code, REG_RAX, REG_RSI); break;
        case 4: emit_load_dword(code, REG_RAX, REG_RSI); break;
        default: emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0); break;
    }
}

static void poke_width(int width) {
    switch (width) {
        case 1: emit_store_byte_reg(code, REG_RSI, REG_RDX); break;
        case 2: emit_store_word(code, REG_RSI, REG_RDX); break;
        case 4: emit_store_dword(code, REG_RSI, REG_RDX); break;
        default: emit_store_mem_disp32(code, REG_RSI, 0, REG_RDX); break;
    }
}

void codegen_builtin_raw_peek(void) {
    codegen_expression();
    expect(TOKEN_COMMA, "expected ',' after address");
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after arguments");
    if (unsupported_target()) {
        emit_add_reg_imm32(code, REG_RSP, 2 * 16);
        emit_mov_reg_imm64(code, REG_RAX, 0);
        codegen_push_rax_as_number();
        return;
    }
    pop_int(REG_RCX);
    pop_int(REG_RSI);
    width_switch("runtime error: raw_peek width must be 1, 2, 4 or 8", peek_width);
    codegen_push_rax_as_number();
}

void codegen_builtin_raw_poke(void) {
    codegen_expression();
    expect(TOKEN_COMMA, "expected ',' after address");
    codegen_expression();
    expect(TOKEN_COMMA, "expected ',' after width");
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after arguments");
    if (!unsupported_target()) {
        pop_int(REG_RDX);
        pop_int(REG_RCX);
        pop_int(REG_RSI);
        width_switch("runtime error: raw_poke width must be 1, 2, 4 or 8", poke_width);
    } else {
        emit_add_reg_imm32(code, REG_RSP, 3 * 16);
    }
    push_nil();
}
