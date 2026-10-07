#include <stdint.h>
#include "codegen/file_builtins.h"
#include "codegen/file_path.h"
#include "codegen/expr.h"
#include "codegen/emit_sse.h"
#include "codegen/value.h"
#include "codegen/platform_file.h"
#include "parser/parser.h"
#include "parser/vars.h"

static void push_number(double v) {
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    if (v == 0.0) {
        emit_pxor_xmm_xmm(code, XMM0);
        emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    } else {
        uint64_t bits;
        __builtin_memcpy(&bits, &v, sizeof(bits));
        emit_mov_reg_imm64(code, REG_RAX, bits);
    }
    emit_push_reg(code, REG_RAX);
}

static void emit_write_or_append(int append) {
    codegen_expression(); // path
    expect(TOKEN_COMMA, "expected ',' after path argument");
    codegen_expression(); // content
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    emit_pop_reg(code, REG_RSI); // content payload
    emit_pop_reg(code, REG_RBX); // ignored
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // content length

    int32_t base = higher_order_scratch_offset();
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RSI); // content_ptr
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RDX); // content_len

    emit_pop_reg(code, REG_RSI); // path payload
    emit_pop_reg(code, REG_RBX); // ignored
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // path length
    filepath_emit_nullterm(code); // RAX = null-terminated path buffer

    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    platform_emit_open_write(code, append);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RAX); // fd

    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 24);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 32);
    platform_emit_write_bytes(code);

    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 40); // fd
    platform_emit_close(code);

    push_number(1.0);
}

void codegen_builtin_write_file(void) { emit_write_or_append(0); }
void codegen_builtin_append_file(void) { emit_write_or_append(1); }

void codegen_builtin_file_exists(void) {
    emit_pop_reg(code, REG_RSI); // path payload
    emit_pop_reg(code, REG_RBX); // ignored
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // length
    filepath_emit_nullterm(code); // RAX = null-terminated path buffer

    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    platform_emit_open_read(code);

    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int not_found = emit_jcc_rel32(code, COND_LT);

    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    platform_emit_close(code);
    push_number(1.0);
    int done = emit_jmp_rel32(code);

    emit_patch_jump(code, not_found);
    push_number(0.0);

    emit_patch_jump(code, done);
}
