#include "codegen/heap.h"
#include "codegen/layout.h"
#include "codegen/win_call.h"
#include "elf/elf_dynamic_call.h"
#include "target.h"

#define HEAP_SIZE (16 * 1024 * 1024)
#define MEM_COMMIT_RESERVE 0x3000u // MEM_COMMIT(0x1000) | MEM_RESERVE(0x2000)
#define PAGE_EXECUTE_READWRITE 0x40u

// Plan B, phase B1: emitted ONCE (jumped over, like errors.c's raise
// routine), called from every allocation site via emit_call_back instead
// of each of the ~27 call sites re-emitting these 4 instructions inline.
// Phase B3/B4: in --link=shared mode this routine's body isn't emitted
// into the executable at all -- it lives in libkilnrt.so instead (see
// heap_emit_alloc_routine_only, called by the separate "compile the
// runtime" driver), and heap_emit_alloc becomes a GOT call instead of a
// local one.
static int alloc_routine_offset = -1;

static void emit_alloc_routine(CodeBuf *code) {
    int skip = emit_jmp_rel32(code);
    alloc_routine_offset = code->count;
    emit_mov_reg_imm64(code, REG_RSI, kiln_heap_ptr_addr());
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0); // rax = heap_ptr (becomes the returned block address)
    emit_add_reg_reg(code, REG_RDI, REG_RAX);         // rdi = heap_ptr + size = new heap_ptr
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RDI);
    emit_ret(code);
    emit_patch_jump(code, skip);
}

// Called by the dedicated "compile the runtime" driver (see
// elf/elf_so_writer.c) to emit this routine into libkilnrt.so's OWN code
// buffer, completely separate from any executable's compile_program.
void heap_emit_alloc_routine_only(CodeBuf *code) {
    emit_alloc_routine(code);
}

int heap_alloc_routine_offset(void) { return alloc_routine_offset; }

// !kiln_is_compiling_so(): while THIS runtime routine's body is itself
// being compiled INTO libkilnrt.so/.dll, its own call sites must stay
// local (same-buffer relative calls), not calls back out to the library
// it's already part of -- see target.h's comment on why this is a
// separate question from link mode. True for both targets: which OS
// decides how the actual call happens (elf_dynamic_call vs
// win_call_runtime_import), not whether it happens at all.
static int is_shared(void) {
    return kiln_get_link_mode() == KILN_LINK_SHARED && !kiln_is_compiling_so();
}

void heap_emit_startup(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        // VirtualAlloc hands back a ready-to-use base directly -- no
        // "query then grow" dance needed the way brk requires.
        win_call_begin(code, 0);
        win_call_arg_imm64(code, 0, 0); // lpAddress = NULL
        win_call_arg_imm64(code, 1, HEAP_SIZE);
        win_call_arg_imm64(code, 2, MEM_COMMIT_RESERVE);
        win_call_arg_imm64(code, 3, PAGE_EXECUTE_READWRITE);
        win_call_import(code, PE_IMPORT_VIRTUAL_ALLOC);
        win_call_end(code);
        emit_mov_reg_imm64(code, REG_RSI, kiln_heap_ptr_addr());
        emit_store_mem_disp32(code, REG_RSI, 0, REG_RAX);
        if (!is_shared()) emit_alloc_routine(code);
        return;
    }

    // brk(0) -- query the current break, becomes our arena's start. This
    // part always happens in the EXECUTABLE, even under --link=shared:
    // the arena and its heap_ptr live in the calling process's own
    // memory regardless of which binary's code manages the bump pointer.
    emit_mov_reg_imm64(code, REG_RAX, 12); // syscall: brk
    emit_mov_reg_imm64(code, REG_RDI, 0);
    emit_syscall(code);

    // heap_ptr = current break (save BEFORE growing -- after the next
    // brk call, RAX holds the new break, not the arena's start)
    emit_mov_reg_imm64(code, REG_RSI, kiln_heap_ptr_addr());
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RAX);

    // brk(current_break + HEAP_SIZE) -- grow once, upfront. Not checking
    // the result: a fixed, modest 16MB request failing is not a case
    // this project's "don't over-engineer error handling for things that
    // can't practically happen" rule asks for defensive code around.
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_add_reg_imm32(code, REG_RDI, HEAP_SIZE);
    emit_mov_reg_imm64(code, REG_RAX, 12);
    emit_syscall(code);

    if (!is_shared()) emit_alloc_routine(code);
}

void heap_emit_alloc(CodeBuf *code) {
    if (is_shared()) {
        if (kiln_get_target() == KILN_TARGET_WINDOWS) win_call_runtime_import(code, RUNTIME_IMPORT_HEAP_ALLOC);
        else elf_dynamic_call(code, RUNTIME_IMPORT_HEAP_ALLOC);
        return;
    }
    emit_call_back(code, alloc_routine_offset);
}
