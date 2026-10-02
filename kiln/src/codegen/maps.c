#include "codegen/maps.h"
#include "codegen/maps_internal.h"
#include "codegen/expr.h"
#include "codegen/strings.h"
#include "codegen/emit_internal.h"
#include "codegen/value.h"
#include "codegen/heap.h"
#include "codegen/errors.h"
#include "parser/parser.h"

void codegen_map_literal(void) {
    advance_token(); // consume '{'
    int count = 0;
    if (current.type != TOKEN_RBRACE) {
        for (;;) {
            if (current.type != TOKEN_STRING) parse_error("expected string key in map literal");
            codegen_string_literal(); // pushes (TAG_STRING, key address)
            expect(TOKEN_COLON, "expected ':' after map key");
            codegen_expression(); // pushes the value
            count++;
            if (current.type != TOKEN_COMMA) break;
            advance_token();
        }
    }
    expect(TOKEN_RBRACE, "expected '}' after map literal");

    emit_mov_reg_imm64(code, REG_RDI, (uint64_t)(count > 0 ? count * MAP_ENTRY_SIZE : 1));
    heap_emit_alloc(code);                      // RAX = entries block
    emit_mov_reg_reg(code, REG_RSI, REG_RAX);   // stash across the pop loop

    // Pairs were pushed key-then-value, left to right, so the LAST
    // pair's value is on top -- pop in reverse to land each pair right.
    for (int i = count - 1; i >= 0; i--) {
        emit_pop_reg(code, REG_RAX); // value payload
        emit_pop_reg(code, REG_RBX); // value tag
        int off = MAP_ENTRY_SIZE * i;
        emit_store_mem_disp32(code, REG_RSI, off + 16, REG_RBX);
        emit_store_mem_disp32(code, REG_RSI, off + 24, REG_RAX);
        emit_pop_reg(code, REG_RAX); // key payload
        emit_pop_reg(code, REG_RBX); // key tag
        emit_store_mem_disp32(code, REG_RSI, off, REG_RBX);
        emit_store_mem_disp32(code, REG_RSI, off + 8, REG_RAX);
    }

    emit_push_reg(code, REG_RSI); // save entries block across this alloc
    emit_mov_reg_imm64(code, REG_RDI, MAP_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = map object
    emit_pop_reg(code, REG_RSI);

    emit_mov_reg_imm64(code, REG_RCX, (uint64_t)count);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);  // capacity
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);  // count
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RSI); // entries_ptr

    emit_mov_reg_imm64(code, REG_RBX, TAG_MAP);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

int find_entry_index(void) {
    emit_mov_reg_imm64(code, REG_RCX, 0); // i

    int loop_start = code->count;
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 8); // count
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int not_found = emit_jcc_rel32(code, COND_GE);

    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 16); // entries_ptr
    emit_mov_reg_reg(code, REG_RBX, REG_RCX);
    emit_push_reg(code, REG_RCX);
    emit_mov_reg_imm64(code, REG_RCX, MAP_ENTRY_SIZE);
    emit_imul_reg_reg(code, REG_RBX, REG_RCX);
    emit_pop_reg(code, REG_RCX);
    emit_add_reg_reg(code, REG_RBX, REG_RDX); // RBX = entry addr

    emit_load_mem_disp32(code, REG_RAX, REG_RBX, 8); // entry key payload (a_payload)
    emit_push_reg(code, REG_RCX); // i
    emit_push_reg(code, REG_RSI); // map object
    emit_push_reg(code, REG_RDI); // target key
    emit_push_reg(code, REG_RBX); // entry addr
    emit_mov_reg_reg(code, REG_RCX, REG_RDI); // b_payload = target key
    codegen_string_compare(0); // pushes (tag=NUMBER, payload=0.0/1.0)
    emit_pop_reg(code, REG_RAX); // compare result payload
    emit_pop_reg(code, REG_RDX); // compare result tag (ignored)
    emit_pop_reg(code, REG_RBX); // entry addr
    emit_pop_reg(code, REG_RDI); // target key
    emit_pop_reg(code, REG_RSI); // map object
    emit_pop_reg(code, REG_RCX); // i
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int is_match = emit_jcc_rel32(code, COND_NE);

    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_jmp_back(code, loop_start);

    emit_patch_jump(code, is_match);
    return not_found;
}

void codegen_map_index_read(void) {
    emit_pop_reg(code, REG_RDI); // key payload
    emit_pop_reg(code, REG_RBX); // key tag (assumed STRING)
    emit_pop_reg(code, REG_RSI); // map object
    emit_pop_reg(code, REG_RBX); // map tag (assumed MAP)

    int not_found = find_entry_index();
    emit_load_mem_disp32(code, REG_RDX, REG_RBX, 16); // value tag
    emit_load_mem_disp32(code, REG_RAX, REG_RBX, 24); // value payload
    emit_push_reg(code, REG_RDX);
    emit_push_reg(code, REG_RAX);
    int done = emit_jmp_rel32(code);

    emit_patch_jump(code, not_found);
    errors_emit_die(code, "runtime error: key not found");

    emit_patch_jump(code, done);
}
