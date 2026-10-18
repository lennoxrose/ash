#include "codegen/H/collections/matrix_mul.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/platform/linux_call.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/runtime/errors.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"
#include "app/H/target.h"

// matrix_mul(a, b) -- real Phase 4 integration (ideas/gpu_acceleration.md,
// ideas/assigned.md): calls INTO libpyre.so's ash_gpu_matrix_multiply
// through the GOT slot elf/C/elf_dynamic.c's --link=shared writer lays
// out, rather than reimplementing the algorithm in kiln's own hand-rolled
// x86-64 the way the CPU backend itself does -- that distinction is the
// whole point (see assigned.md for why).
//
// Only available under --link=shared: that mode is the only one with
// any dynamic-linking machinery at all for a second needed library to
// hang off of (see elf/H/elf_dynamic.h) -- --link=static stays the
// hand-rolled, zero-dependency ELF/PE kiln otherwise always produces,
// unconditionally, which is the entire reason that mode exists.
//
// Square N x N arrays of arrays of numbers only, same N (matches
// ash_gpu_matrix_multiply's own signature exactly, pyre/src/H/runtime/ash_gpu.h --
// general M x K times K x P is a known gap, same one ashvm's own
// matrix_mul documents, not silently assumed away). Marshals by
// flattening into temporary flat double buffers (todo.md 2.A's
// "Contiguous Buffer Type" doesn't exist yet, so this is the bridge
// until it does) -- none of the three temporary buffers are freed
// (kiln has no free() at all yet; the result's own two are returned to
// the caller anyway and would need to stay alive regardless).
//
// Scratch layout (higher_order_scratch_offset(), each field 8 bytes):
//   +0  a_payload      +8  b_payload      +16 n
//   +24 flat_a          +32 flat_b          +40 flat_c
//   +48 i               +56 j               +64 row_payload (scratch)
//   +72 result_payload  +80 c_data (result's own outer data block)
static int32_t scratch_base;
#define S(off) (scratch_base + (off))

static void die(CodeBuf *code) {
    errors_emit_die(code, "runtime error: matrix_mul(a, b) expects two non-empty square arrays of arrays of numbers, same size");
}

// Allocates an n-element data block of (TAG_ARRAY, row) pairs is NOT
// what this builds -- rows are built by flatten/unflatten below, each
// inline where it's needed, since the two directions differ (reading a
// row's own data_ptr vs. allocating one from scratch).

// scratch[S(dst_flat)][i*n+j] = <nested array>.data_ptr[i].payload's row's
// data_ptr[j].payload, for i,j in [0, n) -- flattens `array_scratch_off`
// (an a_payload/b_payload-shaped nested array) into a freshly allocated
// flat buffer, leaving the buffer's address in scratch[dst_flat_off].
static void flatten(CodeBuf *code, int32_t array_scratch_off, int32_t dst_flat_off) {
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, S(16)); // n
    emit_imul_reg_reg(code, REG_RDI, REG_RDI);           // n*n
    emit_mov_reg_imm64(code, REG_RAX, 8);
    emit_imul_reg_reg(code, REG_RDI, REG_RAX);           // n*n*8 bytes
    heap_emit_alloc(code);                                // RAX = flat buffer
    emit_store_mem_disp32(code, REG_RBP, S(dst_flat_off), REG_RAX);

    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, S(48), REG_RCX); // i = 0
    int loop_i = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(48));
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(16));
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int exit_i = emit_jcc_rel32(code, COND_GE);

    // row_payload = array.data_ptr[i].payload
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, S(array_scratch_off));
    emit_load_mem_disp32(code, REG_RSI, REG_RSI, 16); // data_ptr
    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, 16);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RSI, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // row payload
    emit_store_mem_disp32(code, REG_RBP, S(64), REG_RAX);

    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, S(56), REG_RCX); // j = 0
    int loop_j = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(56));
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(16));
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int exit_j = emit_jcc_rel32(code, COND_GE);

    // value = row.data_ptr[j].payload
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, S(64));
    emit_load_mem_disp32(code, REG_RSI, REG_RSI, 16);
    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, 16);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RSI, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // value bits

    // dest = flat + (i*n+j)*8
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, S(dst_flat_off));
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(48)); // i
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, S(16)); // n
    emit_imul_reg_reg(code, REG_RDX, REG_RBX);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(56)); // j
    emit_add_reg_reg(code, REG_RDX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RBX, 8);
    emit_imul_reg_reg(code, REG_RDX, REG_RBX);
    emit_add_reg_reg(code, REG_RSI, REG_RDX);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RAX);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(56));
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, S(56), REG_RCX);
    emit_jmp_back(code, loop_j);
    emit_patch_jump(code, exit_j);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(48));
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, S(48), REG_RCX);
    emit_jmp_back(code, loop_i);
    emit_patch_jump(code, exit_i);
}

// Inverse of flatten(): builds a fresh n x n nested array (header + n
// row headers + n row data blocks) from scratch[40] (flat_c), leaving
// the result's (tag, payload) pushed.
static void unflatten_result(CodeBuf *code) {
    // outer data block: n entries of (TAG_ARRAY, row_payload)
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, S(16));
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RDI, REG_RAX);
    heap_emit_alloc(code); // RAX = outer data block
    emit_store_mem_disp32(code, REG_RBP, S(80), REG_RAX);

    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, S(48), REG_RCX); // i = 0
    int loop_i = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(48));
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(16));
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int exit_i = emit_jcc_rel32(code, COND_GE);

    // row data block: n doubles, copied straight from flat_c's row
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, S(16));
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RDI, REG_RAX);
    heap_emit_alloc(code); // RAX = row data block
    emit_store_mem_disp32(code, REG_RBP, S(64), REG_RAX);

    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, S(56), REG_RCX); // j = 0
    int loop_j = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(56));
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(16));
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int exit_j = emit_jcc_rel32(code, COND_GE);

    // value = flat_c[i*n+j]
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, S(40)); // flat_c
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(48)); // i
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, S(16)); // n
    emit_imul_reg_reg(code, REG_RDX, REG_RBX);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(56)); // j
    emit_add_reg_reg(code, REG_RDX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RBX, 8);
    emit_imul_reg_reg(code, REG_RDX, REG_RBX);
    emit_add_reg_reg(code, REG_RSI, REG_RDX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0); // value bits

    // row_data[j] = (TAG_NUMBER, value)
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, S(64)); // row data block
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(56)); // j
    emit_mov_reg_imm64(code, REG_RBX, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RBX);
    emit_add_reg_reg(code, REG_RSI, REG_RDX);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(56));
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, S(56), REG_RCX);
    emit_jmp_back(code, loop_j);
    emit_patch_jump(code, exit_j);

    // row header: {capacity=n, count=n, data_ptr=row data block}
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = row object
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(16));
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(64));
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RDX);

    // outer[i] = (TAG_ARRAY, row object)
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, S(80));
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(48));
    emit_mov_reg_imm64(code, REG_RDX, 16);
    emit_imul_reg_reg(code, REG_RCX, REG_RDX);
    emit_add_reg_reg(code, REG_RSI, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, TAG_ARRAY);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RDX);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(48));
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, S(48), REG_RCX);
    emit_jmp_back(code, loop_i);
    emit_patch_jump(code, exit_i);

    // outer header: {capacity=n, count=n, data_ptr=outer data block}
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = result object
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(16));
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(80));
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RDX);

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

void codegen_builtin_matrix_mul(void) {
    if (kiln_get_link_mode() != KILN_LINK_SHARED) {
        parse_error("matrix_mul requires --link=shared (it calls into libpyre.so; "
                    "--link=static stays kiln's zero-dependency default -- see ideas/assigned.md)");
    }

    scratch_base = higher_order_scratch_offset();

    // args already pushed left-to-right by codegen/C/builtins/builtins.c's
    // dispatch: [a_tag, a_payload, b_tag, b_payload]
    emit_pop_reg(code, REG_RAX); // b_payload
    emit_pop_reg(code, REG_RBX); // b_tag
    emit_pop_reg(code, REG_RCX); // a_payload
    emit_pop_reg(code, REG_RDX); // a_tag
    emit_cmp_reg_imm32(code, REG_RDX, TAG_ARRAY);
    int a_ok = emit_jcc_rel32(code, COND_E);
    die(code);
    emit_patch_jump(code, a_ok);
    emit_cmp_reg_imm32(code, REG_RBX, TAG_ARRAY);
    int b_ok = emit_jcc_rel32(code, COND_E);
    die(code);
    emit_patch_jump(code, b_ok);

    emit_store_mem_disp32(code, REG_RBP, S(0), REG_RCX); // a_payload
    emit_store_mem_disp32(code, REG_RBP, S(8), REG_RAX); // b_payload

    // n = a.count; must equal b.count, and be > 0
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 8); // a.count
    emit_store_mem_disp32(code, REG_RBP, S(16), REG_RAX);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int n_nonzero = emit_jcc_rel32(code, COND_GT);
    die(code);
    emit_patch_jump(code, n_nonzero);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(8));
    emit_load_mem_disp32(code, REG_RCX, REG_RCX, 8); // b.count
    emit_cmp_reg_reg(code, REG_RAX, REG_RCX);
    int sizes_match = emit_jcc_rel32(code, COND_E);
    die(code);
    emit_patch_jump(code, sizes_match);

    // row0 of a: must be TAG_ARRAY with count == n
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, S(0));
    emit_load_mem_disp32(code, REG_RSI, REG_RSI, 16); // a.data_ptr
    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 0);  // row0 tag
    emit_cmp_reg_imm32(code, REG_RBX, TAG_ARRAY);
    int a_row_ok = emit_jcc_rel32(code, COND_E);
    die(code);
    emit_patch_jump(code, a_row_ok);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // row0 payload
    emit_load_mem_disp32(code, REG_RCX, REG_RAX, 8); // row0 count
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(16));
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int a_row_size_ok = emit_jcc_rel32(code, COND_E);
    die(code);
    emit_patch_jump(code, a_row_size_ok);

    // row0 of b: same check
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, S(8));
    emit_load_mem_disp32(code, REG_RSI, REG_RSI, 16);
    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 0);
    emit_cmp_reg_imm32(code, REG_RBX, TAG_ARRAY);
    int b_row_ok = emit_jcc_rel32(code, COND_E);
    die(code);
    emit_patch_jump(code, b_row_ok);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);
    emit_load_mem_disp32(code, REG_RCX, REG_RAX, 8);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(16));
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int b_row_size_ok = emit_jcc_rel32(code, COND_E);
    die(code);
    emit_patch_jump(code, b_row_size_ok);

    flatten(code, 0, 24);  // flat_a from a_payload
    flatten(code, 8, 32);  // flat_b from b_payload

    // flat_c: n*n*8 bytes, left uninitialized -- ash_gpu_matrix_multiply
    // fully overwrites it itself (see pyre/src/C/backends/cpu/matrix_multiply.c's
    // own memset at the top of its CPU path).
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, S(16));
    emit_imul_reg_reg(code, REG_RDI, REG_RDI);
    emit_mov_reg_imm64(code, REG_RAX, 8);
    emit_imul_reg_reg(code, REG_RDI, REG_RAX);
    heap_emit_alloc(code);
    emit_store_mem_disp32(code, REG_RBP, S(40), REG_RAX);

    linux_abi_call_begin(code);
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, S(24)); // flat_a
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, S(32)); // flat_b
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, S(40)); // flat_c
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, S(16)); // n
    ash_gpu_dynamic_call(code, ASH_GPU_IMPORT_MATRIX_MULTIPLY);
    linux_abi_call_end(code);

    unflatten_result(code);
}
