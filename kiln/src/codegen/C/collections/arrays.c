#include "codegen/H/collections/arrays.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/emit/bytes.h"
#include "codegen/H/runtime/errors.h"
#include "parser/H/core/parser.h"

void codegen_array_literal(void) {
    advance_token(); // consume '['
    int count = 0;
    if (current.type != TOKEN_RBRACKET) {
        codegen_expression(); count++;
        while (current.type == TOKEN_COMMA) { advance_token(); codegen_expression(); count++; }
    }
    expect(TOKEN_RBRACKET, "expected ']' after array literal");

    // elements block first (its address becomes the object's data_ptr)
    emit_mov_reg_imm64(code, REG_RDI, (uint64_t)(count > 0 ? count * 16 : 1));
    heap_emit_alloc(code);                      // RAX = elements block
    emit_mov_reg_reg(code, REG_RSI, REG_RAX);   // stash: RSI survives the pop loop below

    // Elements were pushed left to right, so the last one is on top --
    // pop from the end of the array backward to land each in the right slot.
    for (int i = count - 1; i >= 0; i--) {
        emit_pop_reg(code, REG_RAX); // payload
        emit_pop_reg(code, REG_RBX); // tag
        emit_store_mem_disp32(code, REG_RSI, 16 * i, REG_RBX);
        emit_store_mem_disp32(code, REG_RSI, 16 * i + 8, REG_RAX);
    }

    // now the stable array object
    emit_push_reg(code, REG_RSI); // save elements block address across this alloc
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = array object
    emit_pop_reg(code, REG_RSI);

    emit_mov_reg_imm64(code, REG_RCX, (uint64_t)count);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);  // capacity
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);  // count
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RSI); // data_ptr

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX); // payload = the stable array object's address
}

// Given an array object address (RSI) and a raw index payload (RAX, a
// double's bits), converts to an integer index (RCX), dies with `msg` if
// out of [0, count), and leaves data_ptr in RDX.
static void bounds_check(const char *msg) {
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RCX, XMM0);
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int neg = emit_jcc_rel32(code, COND_LT);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // count
    emit_cmp_reg_reg(code, REG_RCX, REG_RAX);
    int toobig = emit_jcc_rel32(code, COND_GE);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 16); // data_ptr
    int ok = emit_jmp_rel32(code);
    emit_patch_jump(code, neg);
    emit_patch_jump(code, toobig);
    errors_emit_die(code, msg);
    emit_patch_jump(code, ok);
}

void codegen_array_index_read(void) {
    emit_pop_reg(code, REG_RAX); // index payload
    emit_pop_reg(code, REG_RBX); // index tag (assumed NUMBER)
    emit_pop_reg(code, REG_RSI); // array object address
    emit_pop_reg(code, REG_RBX); // array tag (assumed ARRAY)

    bounds_check("runtime error: array index out of bounds"); // RCX=index, RDX=data_ptr

    // element address = data_ptr + RCX*16
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RCX, REG_RAX);
    emit_add_reg_reg(code, REG_RCX, REG_RDX);

    emit_load_mem_disp32(code, REG_RBX, REG_RCX, 0);
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 8);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

void codegen_array_index_store(void) {
    emit_pop_reg(code, REG_RAX); // value payload
    emit_pop_reg(code, REG_RBX); // value tag
    emit_pop_reg(code, REG_RCX); // index payload
    emit_pop_reg(code, REG_RDI); // index tag (ignored)
    emit_pop_reg(code, REG_RSI); // array object address
    emit_pop_reg(code, REG_RDI); // array tag (ignored)

    // stash value on the real stack while bounds_check uses RAX/RCX/RDX
    emit_push_reg(code, REG_RBX); // value tag
    emit_push_reg(code, REG_RAX); // value payload
    emit_mov_reg_reg(code, REG_RAX, REG_RCX); // index payload -> RAX, where bounds_check expects it

    bounds_check("runtime error: array index out of bounds"); // RCX=index, RDX=data_ptr

    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RCX, REG_RAX);
    emit_add_reg_reg(code, REG_RCX, REG_RDX); // element address

    emit_pop_reg(code, REG_RAX); // value payload
    emit_pop_reg(code, REG_RBX); // value tag
    emit_store_mem_disp32(code, REG_RCX, 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RCX, 8, REG_RAX);
}

// Scratch layout (stable base in RCX, same technique as
// codegen_string_concat): [0]=val_tag [8]=val_payload [16]=array_object
void codegen_builtin_push(void) {
    emit_pop_reg(code, REG_RAX); // val payload
    emit_pop_reg(code, REG_RBX); // val tag
    emit_pop_reg(code, REG_RSI); // array object address
    emit_pop_reg(code, REG_RDX); // array tag (ignored)

    emit_sub_reg_imm8(code, REG_RSP, 24);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_store_mem_disp32(code, REG_RCX, 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RCX, 8, REG_RAX);
    emit_store_mem_disp32(code, REG_RCX, 16, REG_RSI);

    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0); // capacity
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 8); // count
    emit_cmp_reg_reg(code, REG_RDX, REG_RAX);
    int has_room = emit_jcc_rel32(code, COND_LT);

    // --- grow: new data block sized capacity+4, copy the old elements over ---
    emit_add_reg_imm8(code, REG_RAX, 4);       // RAX = new_capacity
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_mov_reg_imm64(code, REG_RBX, 16);
    emit_imul_reg_reg(code, REG_RDI, REG_RBX); // RDI = new data block size

    emit_push_reg(code, REG_RCX); // save scratch base across the call
    emit_push_reg(code, REG_RAX); // save new_capacity too (heap_emit_alloc doesn't touch the real stack)
    heap_emit_alloc(code);         // RAX = new data block
    emit_pop_reg(code, REG_RDX);   // RDX = new_capacity
    emit_pop_reg(code, REG_RCX);   // scratch base

    emit_push_reg(code, REG_RAX); // save new data block across bytes_emit_copy
    emit_push_reg(code, REG_RDX); // save new_capacity
    emit_push_reg(code, REG_RCX); // save scratch base

    emit_load_mem_disp32(code, REG_RBX, REG_RCX, 16);  // RBX = array object
    emit_load_mem_disp32(code, REG_RDX, REG_RBX, 16);  // RDX = old data_ptr
    emit_mov_reg_reg(code, REG_RBX, REG_RDX);          // src = old data_ptr
    emit_load_mem_disp32(code, REG_RDX, REG_RCX, 16);  // reload array object (RBX just got overwritten)
    emit_load_mem_disp32(code, REG_RDX, REG_RDX, 0);   // RDX = old capacity
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);          // dest = new data block (about to be overwritten by copy loop, ok, saved on stack too)
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RAX);         // RDX = old_capacity*16 bytes to copy
    bytes_emit_copy(code);

    emit_pop_reg(code, REG_RCX);   // scratch base
    emit_pop_reg(code, REG_RDX);   // new_capacity
    emit_pop_reg(code, REG_RAX);   // new data block

    emit_load_mem_disp32(code, REG_RBX, REG_RCX, 16); // array object
    emit_store_mem_disp32(code, REG_RBX, 0, REG_RDX);  // capacity = new_capacity
    emit_store_mem_disp32(code, REG_RBX, 16, REG_RAX); // data_ptr = new data block

    emit_patch_jump(code, has_room);

    // --- append: element goes at data[count], then count++ ---
    emit_load_mem_disp32(code, REG_RSI, REG_RCX, 16);  // array object (reload -- has_room's jump skips straight here)
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 8);   // count
    emit_load_mem_disp32(code, REG_RDI, REG_RSI, 16);  // data_ptr
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RAX);
    emit_add_reg_reg(code, REG_RDX, REG_RDI); // RDX = new element's address

    emit_load_mem_disp32(code, REG_RBX, REG_RCX, 0); // val_tag
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 8); // val_payload
    emit_store_mem_disp32(code, REG_RDX, 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RDX, 8, REG_RAX);

    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);
    emit_add_reg_imm8(code, REG_RAX, 1);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX); // count++

    emit_add_reg_imm8(code, REG_RSP, 24);

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RSI); // push() returns the array itself, matching ashvm
}

void codegen_builtin_len(void) {
    emit_pop_reg(code, REG_RAX); // payload (array object address)
    emit_pop_reg(code, REG_RBX); // tag (ignored, assumed ARRAY)
    emit_load_mem_disp32(code, REG_RAX, REG_RAX, 8); // count
    emit_cvtsi2sd(code, XMM0, REG_RAX);
    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
