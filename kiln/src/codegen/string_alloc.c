#include "codegen/string_alloc.h"
#include "codegen/heap.h"
#include "codegen/win_call.h"
#include "elf/elf_dynamic_call.h"
#include "target.h"

// Plan B, phase B1: emitted once (jumped over), called via emit_call_back
// from all 13 call sites -- same pattern as codegen/heap.c's alloc
// routine and codegen/bytes.c's copy routine (which this one itself
// calls, via heap_emit_alloc -- routines calling other routines nest
// exactly like any other CALL/RET, no special handling needed). Note this
// stays a LOCAL call even when compiling libkilnrt.so/.dll: the "compile
// the runtime" driver sets kiln_set_compiling_so(1), which
// heap_emit_alloc's own is_shared() check also looks at, so it correctly
// resolves to a same-library relative call rather than calling back out
// to itself (link mode itself stays "shared" throughout -- see target.h).
static int prefixed_routine_offset = -1;

static void emit_prefixed_routine(CodeBuf *code) {
    int skip = emit_jmp_rel32(code);
    prefixed_routine_offset = code->count;
    emit_mov_reg_reg(code, REG_RDI, REG_RDX);
    emit_add_reg_imm8(code, REG_RDI, 8);
    heap_emit_alloc(code); // RAX = block address; RDX (length) survives
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RDX);
    emit_ret(code);
    emit_patch_jump(code, skip);
}

void string_alloc_emit_prefixed_routine_only(CodeBuf *code) {
    emit_prefixed_routine(code);
}

int string_alloc_prefixed_routine_offset(void) { return prefixed_routine_offset; }

static int is_shared(void) {
    return kiln_get_link_mode() == KILN_LINK_SHARED && !kiln_is_compiling_so();
}

void string_alloc_emit_startup(CodeBuf *code) {
    if (!is_shared()) emit_prefixed_routine(code);
}

void string_alloc_emit_prefixed(CodeBuf *code) {
    if (is_shared()) {
        if (kiln_get_target() == KILN_TARGET_WINDOWS) win_call_runtime_import(code, RUNTIME_IMPORT_STRING_ALLOC_PREFIXED);
        else elf_dynamic_call(code, RUNTIME_IMPORT_STRING_ALLOC_PREFIXED);
        return;
    }
    emit_call_back(code, prefixed_routine_offset);
}
