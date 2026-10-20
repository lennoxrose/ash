#include "codegen/H/builtins/mem_builtin.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/strings/strings.h"
#include "codegen/H/collections/arrays.h"
#include "codegen/H/collections/maps.h"
#include "parser/H/core/parser.h"

// mem_release(x): System.free(x) written by the program. kiln cannot tell what kind of block x is (only the
// Strings the compiler itself makes by + or str() are known to be recyclable), so it evaluates x and ignores it.
void codegen_builtin_mem_release(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RAX);
    emit_pop_reg(code, REG_RBX);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NIL);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RBX);
}

static void push_nil(void) {
    emit_mov_reg_imm64(code, REG_RBX, TAG_NIL);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RBX);
}

void codegen_builtin_mem_free(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    if (heap_free_lists_on()) {
        emit_cmp_reg_imm32(code, REG_RBX, TAG_STRING);
        int not_string = emit_jcc_rel32(code, COND_NE);
        heap_emit_free_string_rax(code);
        emit_patch_jump(code, not_string);
    }
    push_nil();
}

// Frees the String whose payload is in RAX when bit `bit` of the flags in RDX is set.
// Keeps RDI and the stack as they were; clobbers RAX, RCX, RDX (when it frees), RSI.
static void free_if_flag(int bit) {
    emit_mov_reg_reg(code, REG_RCX, REG_RDX);
    emit_mov_reg_imm64(code, REG_RSI, (uint64_t)bit);
    emit_and_reg_reg(code, REG_RCX, REG_RSI);
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int skip = emit_jcc_rel32(code, COND_E);
    heap_emit_free_string_rax(code);
    emit_patch_jump(code, skip);
}

void codegen_builtin_mem_cat(void) {
    codegen_expression();
    expect(TOKEN_COMMA, "expected ',' after argument");
    codegen_expression();
    expect(TOKEN_COMMA, "expected ',' after argument");
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RDX); // flags payload (a double)
    emit_pop_reg(code, REG_RBX); // tag
    emit_movq_xmm_from_reg(code, XMM0, REG_RDX);
    emit_cvttsd2si(code, REG_RDX, XMM0);
    emit_pop_reg(code, REG_RCX); // b payload
    emit_pop_reg(code, REG_RBX);
    emit_pop_reg(code, REG_RAX); // a payload
    emit_pop_reg(code, REG_RBX);
    if (!heap_free_lists_on()) {
        codegen_string_concat(); // operands stay where they are
        return;
    }
    emit_push_reg(code, REG_RAX); // a
    emit_push_reg(code, REG_RCX); // b
    emit_push_reg(code, REG_RDX); // flags
    codegen_string_concat();      // pushes the result (tag, payload)
    emit_pop_reg(code, REG_RDI);  // result payload
    emit_pop_reg(code, REG_RBX);  // result tag (a String)
    emit_pop_reg(code, REG_RDX);  // flags
    emit_pop_reg(code, REG_RCX);  // b
    emit_pop_reg(code, REG_RAX);  // a
    emit_push_reg(code, REG_RDI); // result
    emit_push_reg(code, REG_RDX); // flags: the free code clobbers RDX
    emit_push_reg(code, REG_RCX); // b
    free_if_flag(1);              // a is in RAX
    emit_pop_reg(code, REG_RAX);  // b
    emit_pop_reg(code, REG_RDX);  // flags
    free_if_flag(2);
    emit_pop_reg(code, REG_RDI);  // result payload
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RDI);
}

// mem_free_arr(x): gives an array (its object and element block) back to the allocator when the program was
// compiled with array free lists; otherwise evaluated and ignored.
void codegen_builtin_mem_free_arr(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    if (heap_free_arrays_on()) {
        emit_cmp_reg_imm32(code, REG_RBX, TAG_ARRAY);
        int not_array = emit_jcc_rel32(code, COND_NE);
        heap_emit_free_array_rax(code);
        emit_patch_jump(code, not_array);
    }
    push_nil();
}

// mem_newarr1(n, d): a new array of max(n, 0) copies of d with exactly that capacity, so freeing it gives
// back everything it allocated (the push loop it replaces leaves its abandoned growth blocks behind).
// Scratch layout, base in RSI: [0]=d tag [8]=d payload [16]=n [24]=object [32]=data block.
void codegen_builtin_mem_newarr1(void) {
    codegen_expression();
    expect(TOKEN_COMMA, "expected ',' after argument");
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RAX); // d payload
    emit_pop_reg(code, REG_RBX); // d tag
    emit_pop_reg(code, REG_RCX); // n payload (a double)
    emit_pop_reg(code, REG_RDX); // n tag
    emit_movq_xmm_from_reg(code, XMM0, REG_RCX);
    emit_cvttsd2si(code, REG_RCX, XMM0);
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int positive = emit_jcc_rel32(code, COND_GT);
    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_patch_jump(code, positive);
    emit_sub_reg_imm8(code, REG_RSP, 40);
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);
    emit_store_mem_disp32(code, REG_RSI, 16, REG_RCX);

    emit_mov_reg_reg(code, REG_RDI, REG_RCX);
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RDI, REG_RAX);                 // data size = n * 16
    emit_cmp_reg_imm32(code, REG_RDI, 0);
    int sized = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RDI, 1);                      // an empty array still owns a 1-byte block, like `[]`
    emit_patch_jump(code, sized);
    emit_push_reg(code, REG_RSI);
    heap_emit_alloc_recyclable(code);                          // RAX = data block
    emit_pop_reg(code, REG_RSI);
    emit_store_mem_disp32(code, REG_RSI, 32, REG_RAX);

    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    emit_push_reg(code, REG_RSI);
    heap_emit_alloc_recyclable(code);                          // RAX = array object
    emit_pop_reg(code, REG_RSI);
    emit_store_mem_disp32(code, REG_RSI, 24, REG_RAX);

    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 32);          // fill: RBX = element cursor, RCX = remaining
    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 16);
    int loop = code->count;
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int filled = emit_jcc_rel32(code, COND_E);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 0);
    emit_store_mem_disp32(code, REG_RBX, 0, REG_RDX);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 8);
    emit_store_mem_disp32(code, REG_RBX, 8, REG_RDX);
    emit_add_reg_imm8(code, REG_RBX, 16);
    emit_dec_reg(code, REG_RCX);
    emit_jmp_back(code, loop);
    emit_patch_jump(code, filled);

    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 24);          // object
    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 16);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);          // capacity = n
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);          // count = n
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 32);
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RDX);         // data_ptr
    emit_add_reg_imm8(code, REG_RSP, 40);
    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

// mem_free_obj(x): gives an object (a map made by a map literal: its entries block and itself) back to the
// allocator when the program was compiled with block free lists; otherwise evaluated and ignored.
void codegen_builtin_mem_free_obj(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    if (heap_free_arrays_on()) {
        emit_cmp_reg_imm32(code, REG_RBX, TAG_MAP);
        int not_map = emit_jcc_rel32(code, COND_NE);
        heap_emit_free_map_rax(code);
        emit_patch_jump(code, not_map);
    }
    push_nil();
}

// mem_free_map(x): a HashMap / HashSet built by `{}`: the [key, value, next] entry arrays the prelude keeps as its
// values (each one an array literal, so object and element block go back), then the map's entries block and the
// map object. Keys and the program's own values are not touched. Without block free lists: evaluated and ignored.
// Scratch frame (RSP-relative through a copy, since freeing clobbers registers): [0]=cursor [8]=remaining.
void codegen_builtin_mem_free_map(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    if (!heap_free_arrays_on()) { push_nil(); return; }
    emit_cmp_reg_imm32(code, REG_RBX, TAG_MAP);
    int not_map = emit_jcc_rel32(code, COND_NE);

    emit_push_reg(code, REG_RAX);                       // the map object, kept for the final free
    emit_sub_reg_imm8(code, REG_RSP, 16);
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_load_mem_disp32(code, REG_RCX, REG_RAX, 16);   // entries_ptr
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RCX);
    emit_load_mem_disp32(code, REG_RCX, REG_RAX, 8);    // count
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RCX);

    int entry_loop = code->count;
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 8);
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int all_done = emit_jcc_rel32(code, COND_E);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 0);    // cursor
    emit_load_mem_disp32(code, REG_RAX, REG_RDX, 16);   // value tag
    emit_cmp_reg_imm32(code, REG_RAX, TAG_ARRAY);
    int not_chain = emit_jcc_rel32(code, COND_NE);      // the element count is a number
    emit_load_mem_disp32(code, REG_RAX, REG_RDX, 24);   // chain head: an entry array

    int chain_loop = code->count;
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int chain_end = emit_jcc_rel32(code, COND_E);
    emit_load_mem_disp32(code, REG_RCX, REG_RAX, 8);    // an entry array has exactly [key, value, next]
    emit_cmp_reg_imm32(code, REG_RCX, 3);
    int chain_odd = emit_jcc_rel32(code, COND_NE);
    emit_load_mem_disp32(code, REG_RCX, REG_RAX, 16);   // its element block
    emit_load_mem_disp32(code, REG_RDX, REG_RCX, 40);   // next payload
    emit_load_mem_disp32(code, REG_RCX, REG_RCX, 32);   // next tag
    emit_push_reg(code, REG_RDX);
    emit_push_reg(code, REG_RCX);
    heap_emit_free_array_rax(code);                     // RAX = this entry array
    emit_pop_reg(code, REG_RCX);                        // next tag
    emit_pop_reg(code, REG_RAX);                        // next payload
    emit_cmp_reg_imm32(code, REG_RCX, TAG_ARRAY);
    emit_jcc_back(code, COND_E, chain_loop);
    emit_patch_jump(code, chain_end);
    emit_patch_jump(code, chain_odd);
    emit_patch_jump(code, not_chain);

    emit_mov_reg_reg(code, REG_RSI, REG_RSP);           // next entry
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0);
    emit_add_reg_imm8(code, REG_RAX, MAP_ENTRY_SIZE);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);
    emit_dec_reg(code, REG_RAX);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);
    emit_jmp_back(code, entry_loop);

    emit_patch_jump(code, all_done);
    emit_add_reg_imm8(code, REG_RSP, 16);
    emit_pop_reg(code, REG_RAX);                        // the map object
    heap_emit_free_map_rax(code);
    emit_patch_jump(code, not_map);
    push_nil();
}

// mem_free_elems(x): the Strings of a String[] / List<String> whose elements are all fresh concatenations only
// (or the rows of a T[][] whose rows only it holds)
// it references (front/src/jown) go back to the allocator; the array itself is freed by mem_free_arr afterwards.
// Anything else, and programs without free lists, is evaluated and ignored.
// Scratch frame (RSP-relative through a copy, since freeing clobbers registers): [0]=cursor [8]=remaining.
void codegen_builtin_mem_free_elems(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    if (!heap_free_lists_on()) { push_nil(); return; }
    emit_cmp_reg_imm32(code, REG_RBX, TAG_ARRAY);
    int not_array = emit_jcc_rel32(code, COND_NE);

    emit_sub_reg_imm8(code, REG_RSP, 16);
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_load_mem_disp32(code, REG_RCX, REG_RAX, 16);   // data_ptr
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RCX);
    emit_load_mem_disp32(code, REG_RCX, REG_RAX, 8);    // count
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RCX);

    int loop = code->count;
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 8);
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int done = emit_jcc_rel32(code, COND_E);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 0);    // cursor
    emit_load_mem_disp32(code, REG_RAX, REG_RDX, 0);    // element tag
    emit_cmp_reg_imm32(code, REG_RAX, TAG_STRING);
    int not_string = emit_jcc_rel32(code, COND_NE);     // none and the like stay
    emit_load_mem_disp32(code, REG_RAX, REG_RDX, 8);    // the String's payload
    heap_emit_free_string_rax(code);
    int elem_done = emit_jmp_rel32(code);
    emit_patch_jump(code, not_string);
    if (heap_free_arrays_on()) {                        // a T[][]: the rows (made by mem_newarr1 / literals) go back too
        emit_cmp_reg_imm32(code, REG_RAX, TAG_ARRAY);
        int not_row = emit_jcc_rel32(code, COND_NE);
        emit_load_mem_disp32(code, REG_RAX, REG_RDX, 8);
        heap_emit_free_array_rax(code);
        emit_patch_jump(code, not_row);
    }
    emit_patch_jump(code, elem_done);
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0);
    emit_add_reg_imm8(code, REG_RAX, 16);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);
    emit_dec_reg(code, REG_RAX);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);
    emit_jmp_back(code, loop);

    emit_patch_jump(code, done);
    emit_add_reg_imm8(code, REG_RSP, 16);
    emit_patch_jump(code, not_array);
    push_nil();
}

void codegen_builtin_mem_region_enter(void) {
    expect(TOKEN_RPAREN, "expected ')' after 'mem_region_enter('");
    heap_emit_checkpoint(code);                                  // RAX = heap pointer
    // kept as the raw address in a number slot: nothing ever computes with it, and a round trip through a double
    // (two conversions on the path of every iteration) costs more than the allocation it brackets
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

void codegen_builtin_mem_region_leave(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RDI);                                 // payload: the checkpoint, the raw address
    emit_pop_reg(code, REG_RBX);
    heap_emit_region_reset(code);
    push_nil();
}
