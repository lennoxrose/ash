#include "codegen/H/collections/arrays.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/runtime/errors.h"
#include "parser/H/core/parser.h"

// Array element = 16 bytes (tag, payload); object = [capacity][count][data_ptr].
// See codegen/H/emit/value.h.

// RAX = index (a double's bits) -> RCX = integer index.
static void index_to_int(void) {
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RCX, XMM0);
}

// Dies unless 0 <= RCX < count (or <= count when `allow_end`); RSI = array object.
static void check_index(int allow_end, const char *msg) {
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int neg = emit_jcc_rel32(code, COND_LT);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 8); // count
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int big = emit_jcc_rel32(code, allow_end ? COND_GT : COND_GE);
    int ok = emit_jmp_rel32(code);
    emit_patch_jump(code, neg);
    emit_patch_jump(code, big);
    errors_emit_die(code, msg);
    emit_patch_jump(code, ok);
}

// Called by codegen_builtin_delete when its first argument is an array.
// In: RDI = index payload, RSI = array object.
void codegen_array_delete_index(void) {
    emit_mov_reg_reg(code, REG_RAX, REG_RDI);
    index_to_int();
    check_index(0, "index out of bounds");

    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 8);  // count
    emit_sub_reg_reg(code, REG_RBX, REG_RCX);
    emit_sub_reg_imm8(code, REG_RBX, 1);               // items to shift left
    emit_load_mem_disp32(code, REG_RDI, REG_RSI, 16); // data_ptr
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RCX, REG_RAX);
    emit_add_reg_reg(code, REG_RDI, REG_RCX);         // &data[index]

    int loop = code->count;
    emit_cmp_reg_imm32(code, REG_RBX, 0);
    int done = emit_jcc_rel32(code, COND_E);
    emit_load_mem_disp32(code, REG_RAX, REG_RDI, 16); emit_store_mem_disp32(code, REG_RDI, 0, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RDI, 24); emit_store_mem_disp32(code, REG_RDI, 8, REG_RAX);
    emit_add_reg_imm8(code, REG_RDI, 16);
    emit_dec_reg(code, REG_RBX);
    emit_jmp_back(code, loop);
    emit_patch_jump(code, done);

    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);
    emit_sub_reg_imm8(code, REG_RAX, 1);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX); // count--

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RSI); // delete() returns the container, like for maps
}

void codegen_builtin_pop(void) {
    emit_pop_reg(code, REG_RSI); // array object
    emit_pop_reg(code, REG_RBX); // tag (ignored)

    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // count
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int nonempty = emit_jcc_rel32(code, COND_NE);
    errors_emit_die(code, "runtime error: pop() on an empty array");
    emit_patch_jump(code, nonempty);

    emit_sub_reg_imm8(code, REG_RAX, 1);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX); // count--
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 16); // data_ptr
    emit_mov_reg_imm64(code, REG_RCX, 16);
    emit_imul_reg_reg(code, REG_RAX, REG_RCX);
    emit_add_reg_reg(code, REG_RAX, REG_RDX);          // &data[old count - 1]
    emit_load_mem_disp32(code, REG_RBX, REG_RAX, 0);
    emit_load_mem_disp32(code, REG_RAX, REG_RAX, 8);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

// insert(array, index, value): validates the index, appends via push() (so
// growth is shared), then shifts the tail right and drops the value in.
void codegen_builtin_insert(void) {
    emit_pop_reg(code, REG_RAX); // value payload
    emit_pop_reg(code, REG_RBX); // value tag
    emit_pop_reg(code, REG_RCX); // index payload
    emit_pop_reg(code, REG_RDX); // index tag (ignored)
    emit_pop_reg(code, REG_RSI); // array object
    emit_pop_reg(code, REG_RDI); // array tag

    emit_push_reg(code, REG_RAX); // keep value payload across the index check
    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    index_to_int();
    check_index(1, "index out of bounds");
    emit_pop_reg(code, REG_RAX);

    emit_push_reg(code, REG_RBX); // saved: value tag
    emit_push_reg(code, REG_RAX); //        value payload
    emit_push_reg(code, REG_RCX); //        index
    emit_push_reg(code, REG_RDI); // push() arguments: array, then value
    emit_push_reg(code, REG_RSI);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
    codegen_builtin_push();
    emit_pop_reg(code, REG_RSI); // array object (push() returns it)
    emit_pop_reg(code, REG_RDI); // tag
    emit_pop_reg(code, REG_RCX); // index

    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 8);  // new count
    emit_sub_reg_imm8(code, REG_RBX, 1);
    emit_mov_reg_reg(code, REG_RDX, REG_RBX);          // last index
    emit_sub_reg_reg(code, REG_RBX, REG_RCX);          // items to shift right
    emit_load_mem_disp32(code, REG_RDI, REG_RSI, 16); // data_ptr
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RAX);
    emit_add_reg_reg(code, REG_RDI, REG_RDX);          // &data[last]

    int loop = code->count;
    emit_cmp_reg_imm32(code, REG_RBX, 0);
    int done = emit_jcc_rel32(code, COND_E);
    emit_load_mem_disp32(code, REG_RAX, REG_RDI, -16); emit_store_mem_disp32(code, REG_RDI, 0, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RDI, -8);  emit_store_mem_disp32(code, REG_RDI, 8, REG_RAX);
    emit_sub_reg_imm8(code, REG_RDI, 16);
    emit_dec_reg(code, REG_RBX);
    emit_jmp_back(code, loop);
    emit_patch_jump(code, done);

    emit_pop_reg(code, REG_RAX); // value payload
    emit_pop_reg(code, REG_RBX); // value tag
    emit_store_mem_disp32(code, REG_RDI, 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RDI, 8, REG_RAX);

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RSI);
}

// slice(array, from, to): a new array of items [from, to).
void codegen_builtin_slice(void) {
    emit_pop_reg(code, REG_RAX); // to payload
    emit_pop_reg(code, REG_RBX);
    emit_pop_reg(code, REG_RCX); // from payload
    emit_pop_reg(code, REG_RBX);
    emit_pop_reg(code, REG_RSI); // array object
    emit_pop_reg(code, REG_RBX);

    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RDX, XMM0); // to
    emit_movq_xmm_from_reg(code, XMM0, REG_RCX);
    emit_cvttsd2si(code, REG_RCX, XMM0); // from

    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int bad1 = emit_jcc_rel32(code, COND_LT);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // count
    emit_cmp_reg_reg(code, REG_RDX, REG_RAX);
    int bad2 = emit_jcc_rel32(code, COND_GT);
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int bad3 = emit_jcc_rel32(code, COND_GT);
    int ok = emit_jmp_rel32(code);
    emit_patch_jump(code, bad1);
    emit_patch_jump(code, bad2);
    emit_patch_jump(code, bad3);
    errors_emit_die(code, "runtime error: slice bounds out of range");
    emit_patch_jump(code, ok);

    emit_sub_reg_reg(code, REG_RDX, REG_RCX); // n
    emit_push_reg(code, REG_RSI);
    emit_push_reg(code, REG_RCX); // from
    emit_push_reg(code, REG_RDX); // n

    emit_mov_reg_reg(code, REG_RDI, REG_RDX);
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RDI, REG_RAX);
    emit_add_reg_imm8(code, REG_RDI, 16); // room for one spare slot (n may be 0)
    heap_emit_alloc(code);                 // RAX = data block
    emit_push_reg(code, REG_RAX);
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code);                 // RAX = array object

    emit_pop_reg(code, REG_RBX); // data block
    emit_pop_reg(code, REG_RCX); // n
    emit_pop_reg(code, REG_RDX); // from
    emit_pop_reg(code, REG_RSI); // source array

    emit_mov_reg_reg(code, REG_RDI, REG_RCX);
    emit_add_reg_imm8(code, REG_RDI, 1);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RDI);  // capacity = n + 1
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);  // count = n
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RBX); // data_ptr

    emit_load_mem_disp32(code, REG_RSI, REG_RSI, 16);  // source data_ptr
    emit_mov_reg_imm64(code, REG_RDI, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RDI);
    emit_add_reg_reg(code, REG_RSI, REG_RDX);          // &source[from]
    emit_mov_reg_reg(code, REG_RDI, REG_RBX);          // dest cursor

    int loop = code->count;
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int done = emit_jcc_rel32(code, COND_E);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 0); emit_store_mem_disp32(code, REG_RDI, 0, REG_RDX);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 8); emit_store_mem_disp32(code, REG_RDI, 8, REG_RDX);
    emit_add_reg_imm8(code, REG_RSI, 16);
    emit_add_reg_imm8(code, REG_RDI, 16);
    emit_dec_reg(code, REG_RCX);
    emit_jmp_back(code, loop);
    emit_patch_jump(code, done);

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
