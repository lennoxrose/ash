#include "codegen/H/strings/strings.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/emit/bytes.h"
#include "codegen/H/strings/string_alloc.h"
#include "codegen/H/runtime/errors.h"
#include "codegen/H/emit/layout.h"
#include "parser/H/core/parser.h"

static void push_number_literal(double v) {
    uint64_t bits;
    __builtin_memcpy(&bits, &v, sizeof(bits));
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_mov_reg_imm64(code, REG_RAX, bits);
    emit_push_reg(code, REG_RAX);
}

void codegen_string_literal_bytes(const char *text, int len) {
    int actual_len = 0;
    for (int i = 0; i < len; i++) {
        if (text[i] == '\\' && i + 1 < len) i++;
        actual_len++;
    }

    int skip_jump = emit_jmp_rel32(code); // don't execute the embedded data as instructions
    int data_offset = code->count;
    emit_u64(code, (uint64_t)actual_len);
    for (int i = 0; i < len; i++) {
        char c = text[i];
        if (c == '\\' && i + 1 < len) {
            char next = text[i + 1];
            switch (next) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                default: c = next; break;
            }
            i++;
        }
        emit_byte(code, (uint8_t)c);
    }
    emit_patch_jump(code, skip_jump);

    uint64_t payload_addr = kiln_code_base() + (uint64_t)data_offset + 8;
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_mov_reg_imm64(code, REG_RAX, payload_addr);
    emit_push_reg(code, REG_RAX);
}

void codegen_string_literal(void) {
    codegen_string_literal_bytes(current.start, current.length);
    advance_token();
}

// Scratch layout (a stable base pointer copied from RSP into RSI, since
// [rsp+disp] itself needs a SIB byte this project's encoders don't
// support -- same technique codegen/C/runtime/print_int.c uses):
//   [0]=a_payload  [8]=b_payload  [16]=length_a  [24]=length_b  [32]=new_block
void codegen_string_concat(void) {
    emit_sub_reg_imm8(code, REG_RSP, 40);
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);

    emit_store_mem_disp32(code, REG_RSI, 0, REG_RAX);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RCX);

    emit_load_mem_disp32(code, REG_RAX, REG_RAX, -8); // length_a (length prefix sits right before the payload)
    emit_store_mem_disp32(code, REG_RSI, 16, REG_RAX);
    emit_load_mem_disp32(code, REG_RCX, REG_RCX, -8); // length_b
    emit_store_mem_disp32(code, REG_RSI, 24, REG_RCX);

    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_add_reg_reg(code, REG_RDI, REG_RCX);
    emit_add_reg_imm8(code, REG_RDI, 8); // +8 for the new string's own length prefix

    emit_push_reg(code, REG_RSI); // heap_emit_alloc clobbers RSI -- save/restore around it
    heap_emit_alloc(code);         // RAX = new block
    emit_pop_reg(code, REG_RSI);
    emit_store_mem_disp32(code, REG_RSI, 32, REG_RAX);

    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 16);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 24);
    emit_add_reg_reg(code, REG_RCX, REG_RDX);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX); // [new_block] = combined length

    // copy A right after the length prefix
    emit_load_mem_disp32(code, REG_RDI, REG_RSI, 32);
    emit_add_reg_imm8(code, REG_RDI, 8);
    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 0);  // src = a_payload
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 16); // count = length_a
    bytes_emit_copy(code);

    // copy B right after A
    emit_load_mem_disp32(code, REG_RDI, REG_RSI, 32);
    emit_add_reg_imm8(code, REG_RDI, 8);
    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 16); // length_a
    emit_add_reg_reg(code, REG_RDI, REG_RCX);
    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 8);  // src = b_payload
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 24); // count = length_b
    bytes_emit_copy(code);

    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 32);
    emit_add_reg_imm8(code, REG_RAX, 8); // RAX = result payload address

    emit_add_reg_imm8(code, REG_RSP, 40);

    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

void codegen_string_compare(int invert) {
    emit_load_mem_disp32(code, REG_RBX, REG_RAX, -8); // length_a
    emit_load_mem_disp32(code, REG_RDX, REG_RCX, -8); // length_b
    emit_cmp_reg_reg(code, REG_RBX, REG_RDX);
    int to_false = emit_jcc_rel32(code, COND_NE); // different lengths -> can't be equal

    int loop_start = code->count;
    emit_cmp_reg_imm32(code, REG_RBX, 0);
    int to_true = emit_jcc_rel32(code, COND_E); // ran out of bytes with no mismatch -> equal
    emit_load_byte_reg(code, REG_RSI, REG_RAX);
    emit_load_byte_reg(code, REG_RDI, REG_RCX);
    emit_cmp_reg_reg(code, REG_RSI, REG_RDI);
    int mismatch = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RAX, 1);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_dec_reg(code, REG_RBX);
    emit_jmp_back(code, loop_start);

    emit_patch_jump(code, mismatch);
    emit_patch_jump(code, to_false);
    push_number_literal(invert ? 1.0 : 0.0);
    int skip_true = emit_jmp_rel32(code);
    emit_patch_jump(code, to_true);
    push_number_literal(invert ? 0.0 : 1.0);
    emit_patch_jump(code, skip_true);
}

void codegen_string_index_read(void) {
    emit_pop_reg(code, REG_RAX); // index payload
    emit_pop_reg(code, REG_RBX); // index tag (ignored, assumed NUMBER)
    emit_pop_reg(code, REG_RSI); // string payload
    emit_pop_reg(code, REG_RBX); // string tag (ignored, assumed STRING)

    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RCX, XMM0); // index
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // length

    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int neg = emit_jcc_rel32(code, COND_LT);
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int toobig = emit_jcc_rel32(code, COND_GE);
    int ok = emit_jmp_rel32(code);
    emit_patch_jump(code, neg);
    emit_patch_jump(code, toobig);
    errors_emit_die(code, "runtime error: string index out of bounds");
    emit_patch_jump(code, ok);

    emit_add_reg_reg(code, REG_RSI, REG_RCX); // char address
    emit_load_byte_reg(code, REG_RBX, REG_RSI); // char byte (RBX survives string_alloc_emit_prefixed)

    emit_mov_reg_imm64(code, REG_RDX, 1);
    string_alloc_emit_prefixed(code); // RAX = new 1-byte block, length prefix already written

    emit_add_reg_imm8(code, REG_RAX, 8); // payload
    emit_store_byte_reg(code, REG_RAX, REG_RBX);

    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
