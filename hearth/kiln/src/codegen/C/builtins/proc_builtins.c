#include <stdint.h>
#include "codegen/H/builtins/proc_builtins.h"
#include "codegen/H/io/file_path.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/emit/bytes.h"
#include "codegen/H/collections/arrays.h"
#include "codegen/H/runtime/errors.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/strings/string_alloc.h"
#include "app/H/target.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"

// Process builtins for the Ash OS userland. Linux x86-64 only.
// Values are (tag, payload) pairs, payload on top of the stack; a number's
// payload is a double's bit pattern.

// RAX = int64 -> pushes it as a NUMBER.
void codegen_push_rax_as_number(void) {
    emit_cvtsi2sd(code, XMM0, REG_RAX);
    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

// Pops a string and leaves its null-terminated copy's address in RAX.
static void pop_path_nullterm(void) {
    emit_pop_reg(code, REG_RSI);
    emit_pop_reg(code, REG_RBX); // tag (ignored)
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8);
    filepath_emit_nullterm(code);
}

static int unsupported_target(void) {
    if (kiln_get_target() != KILN_TARGET_WINDOWS) return 0;
    errors_emit_die(code, "runtime error: process builtins are Linux only");
    return 1;
}

void codegen_builtin_sys_fork(void) {
    expect(TOKEN_RPAREN, "expected ')' after 'sys_fork('");
    if (!unsupported_target()) {
        emit_mov_reg_imm64(code, REG_RAX, 57); // syscall: fork
        emit_syscall(code);
    } else {
        emit_mov_reg_imm64(code, REG_RAX, 0);
    }
    codegen_push_rax_as_number();
}

void codegen_builtin_sys_wait(void) {
    codegen_expression(); // pid
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RAX); // payload (double bits)
    emit_pop_reg(code, REG_RBX); // tag (ignored)
    if (unsupported_target()) {
        emit_mov_reg_imm64(code, REG_RAX, 0);
        codegen_push_rax_as_number();
        return;
    }
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RDI, XMM0);
    emit_mov_reg_imm64(code, REG_RSI, 0); // status = NULL
    emit_mov_reg_imm64(code, REG_RDX, 0); // options = 0
    // wait4's 4th argument (rusage) lives in R10, which kiln cannot encode.
    // It is 0 anyway: Linux zeroes it at process start and kiln never writes
    // r8-r15 (see emit.h), and fork() keeps it.
    emit_mov_reg_imm64(code, REG_RAX, 61); // syscall: wait4
    emit_syscall(code);
    codegen_push_rax_as_number();
}

void codegen_builtin_sys_exec(void) {
    codegen_expression(); // path
    expect(TOKEN_COMMA, "expected ',' after path");
    codegen_expression(); // arg
    expect(TOKEN_RPAREN, "expected ')' after arguments");
    int32_t base = higher_order_scratch_offset();
    if (unsupported_target()) {
        emit_pop_reg(code, REG_RAX); emit_pop_reg(code, REG_RBX);
        emit_pop_reg(code, REG_RAX); emit_pop_reg(code, REG_RBX);
        emit_mov_reg_imm64(code, REG_RAX, 0);
        codegen_push_rax_as_number();
        return;
    }
    pop_path_nullterm();                                      // arg
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RAX);
    pop_path_nullterm();                                      // path
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // argv[0]

    // argv[1] = arg, or NULL if arg is the empty string
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 24);
    emit_load_byte_reg(code, REG_RCX, REG_RAX);
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int nonempty = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RAX, 0);
    emit_patch_jump(code, nonempty);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RAX);
    emit_mov_reg_imm64(code, REG_RAX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RAX); // argv terminator

    // execve(path, argv, NULL)
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 32);
    emit_mov_reg_reg(code, REG_RSI, REG_RBP);
    emit_mov_reg_imm64(code, REG_RCX, (uint64_t)(int64_t)(base + 32));
    emit_add_reg_reg(code, REG_RSI, REG_RCX);                 // RSI = &argv[0]
    emit_mov_reg_imm64(code, REG_RDX, 0);
    emit_mov_reg_imm64(code, REG_RAX, 59);                    // syscall: execve
    emit_syscall(code);
    codegen_push_rax_as_number();                                     // only reached on failure
}