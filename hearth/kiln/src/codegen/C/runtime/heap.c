#include "codegen/H/runtime/heap.h"
#include "codegen/H/emit/layout.h"
#include "codegen/H/platform/win_call.h"
#include "elf/H/elf_dynamic_call.h"
#include "app/H/target.h"

// Chunk-based growth, replacing the old single fixed 16MB grab: the arena
// starts with one chunk and the alloc routine below grows it by another
// chunk (rounding the request up to a chunk boundary, so one allocation
// bigger than a single chunk still gets everything it needs in one grow
// step) whenever the bump pointer would cross the committed limit. Must be
// a power of two -- the round-up uses an AND mask, not a division.
#define HEAP_CHUNK_SIZE (64u * 1024 * 1024)

// Windows only: VirtualAlloc has no brk-style "extend the current
// region" -- growing in place requires the address range to already be
// reserved, so startup reserves this much virtual address space up front
// (cheap: MEM_RESERVE alone commits no physical memory) and each growth
// step just commits one more chunk inside it. 4GB comfortably covers
// anything this project's current workloads (matrix ops and beyond) need;
// if a program ever outgrows it, that is a real resource ceiling worth
// hitting deliberately rather than silently wrapping into unreserved
// address space.
#define HEAP_RESERVE_SIZE (4ULL * 1024 * 1024 * 1024)

#define MEM_COMMIT 0x1000u
#define MEM_RESERVE 0x2000u
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

// Callers of heap_emit_alloc (~27 sites) rely on a fixed clobber set:
// RAX (return value) and RDI (consumes the size argument) change, RSI is
// explicitly documented as clobbered, but RBX/RCX/RDX/RBP/RSP must come
// back exactly as they went in (several call sites read RCX/RDX straight
// through a call with no reload). The growth path below needs scratch
// registers to talk to brk/VirtualAlloc, so it saves and restores
// RAX/RDI (which this routine's own fast path is still using) plus
// RBX/RCX/RDX (borrowed as scratch) around that one-off syscall/API call,
// instead of changing the contract for the other ~26 sites that never hit
// the slow path at all.
static void emit_alloc_routine(CodeBuf *code) {
    int skip = emit_jmp_rel32(code);
    alloc_routine_offset = code->count;

    emit_mov_reg_imm64(code, REG_RSI, kiln_heap_ptr_addr());
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0); // rax = heap_ptr (the block this call returns)
    emit_add_reg_reg(code, REG_RDI, REG_RAX);         // rdi = heap_ptr + size = new heap_ptr

    emit_mov_reg_imm64(code, REG_RSI, kiln_heap_limit_addr());
    emit_load_mem_disp32(code, REG_RSI, REG_RSI, 0); // rsi = current committed limit

    emit_cmp_reg_reg(code, REG_RDI, REG_RSI); // new heap_ptr vs limit
    int no_growth = emit_jcc_rel32(code, COND_LE); // common case: still inside the committed region

    // ---- slow path: grow by whole chunks, rounding the new pointer up
    // to the next chunk boundary so a single oversized allocation still
    // gets enough room in one step. ----
    emit_push_reg(code, REG_RAX); // old heap_ptr (this call's return value)
    emit_push_reg(code, REG_RDI); // new heap_ptr
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RCX);
    emit_push_reg(code, REG_RDX);

    emit_mov_reg_reg(code, REG_RBX, REG_RDI);
    emit_add_reg_imm32(code, REG_RBX, (int32_t)(HEAP_CHUNK_SIZE - 1));
    emit_mov_reg_imm64(code, REG_RDX, ~(uint64_t)(HEAP_CHUNK_SIZE - 1));
    emit_and_reg_reg(code, REG_RBX, REG_RDX); // rbx = new committed limit (chunk-rounded)

    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        emit_mov_reg_reg(code, REG_RCX, REG_RSI); // rcx = old limit = commit base (already reserved at startup)
        emit_mov_reg_reg(code, REG_RAX, REG_RBX);
        emit_sub_reg_reg(code, REG_RAX, REG_RCX); // rax = size to commit

        win_call_begin(code, 0);
        win_call_arg_reg(code, 0, REG_RCX); // lpAddress = old limit
        win_call_arg_reg(code, 1, REG_RAX); // dwSize
        win_call_arg_imm64(code, 2, MEM_COMMIT);
        win_call_arg_imm64(code, 3, PAGE_EXECUTE_READWRITE);
        win_call_import(code, PE_IMPORT_VIRTUAL_ALLOC);
        win_call_end(code);
    } else {
        // brk(new_limit) -- not checking the result, same "a fixed
        // request failing isn't a case to write defensive code around"
        // call as the one-time startup grab below.
        emit_mov_reg_reg(code, REG_RDI, REG_RBX);
        emit_mov_reg_imm64(code, REG_RAX, 12); // syscall: brk
        emit_syscall(code);
    }

    emit_mov_reg_imm64(code, REG_RSI, kiln_heap_limit_addr());
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RBX); // heap_limit = new committed limit

    emit_pop_reg(code, REG_RDX);
    emit_pop_reg(code, REG_RCX);
    emit_pop_reg(code, REG_RBX);
    emit_pop_reg(code, REG_RDI); // new heap_ptr, restored
    emit_pop_reg(code, REG_RAX); // old heap_ptr, restored

    emit_patch_jump(code, no_growth);
    emit_mov_reg_imm64(code, REG_RSI, kiln_heap_ptr_addr());
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RDI); // heap_ptr = new heap_ptr
    emit_ret(code);
    emit_patch_jump(code, skip);
}

// Called by the dedicated "compile the runtime" driver (see
// elf/C/elf_so_writer.c) to emit this routine into libkilnrt.so's OWN code
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
        // Reserve a large virtual range up front (no physical memory
        // committed -- MEM_RESERVE alone), then commit only the first
        // chunk. Growth later commits further chunks inside this same
        // reservation, so the whole arena stays one contiguous region
        // and existing pointers into it are never invalidated.
        win_call_begin(code, 0);
        win_call_arg_imm64(code, 0, 0); // lpAddress = NULL, let the OS place it
        win_call_arg_imm64(code, 1, HEAP_RESERVE_SIZE);
        win_call_arg_imm64(code, 2, MEM_RESERVE);
        win_call_arg_imm64(code, 3, PAGE_EXECUTE_READWRITE);
        win_call_import(code, PE_IMPORT_VIRTUAL_ALLOC);
        win_call_end(code);
        emit_mov_reg_reg(code, REG_RBX, REG_RAX); // rbx = base (survives the next win_call: non-volatile in the Windows x64 ABI)

        win_call_begin(code, 0);
        win_call_arg_reg(code, 0, REG_RBX); // lpAddress = base
        win_call_arg_imm64(code, 1, HEAP_CHUNK_SIZE);
        win_call_arg_imm64(code, 2, MEM_COMMIT);
        win_call_arg_imm64(code, 3, PAGE_EXECUTE_READWRITE);
        win_call_import(code, PE_IMPORT_VIRTUAL_ALLOC);
        win_call_end(code);

        emit_mov_reg_imm64(code, REG_RSI, kiln_heap_ptr_addr());
        emit_store_mem_disp32(code, REG_RSI, 0, REG_RBX); // heap_ptr = base
        emit_mov_reg_reg(code, REG_RAX, REG_RBX);
        emit_add_reg_imm32(code, REG_RAX, (int32_t)HEAP_CHUNK_SIZE);
        emit_mov_reg_imm64(code, REG_RSI, kiln_heap_limit_addr());
        emit_store_mem_disp32(code, REG_RSI, 0, REG_RAX); // heap_limit = base + one chunk
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

    // brk(current_break + HEAP_CHUNK_SIZE) -- grow once, upfront, to the
    // first chunk. Not checking the result: a modest first request
    // failing is not a case this project's "don't over-engineer error
    // handling for things that can't practically happen" rule asks for
    // defensive code around (later growth steps, which are far more
    // likely to eventually hit real memory pressure, still go through
    // the same unchecked brk -- see emit_alloc_routine).
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_add_reg_imm32(code, REG_RDI, (int32_t)HEAP_CHUNK_SIZE);
    emit_mov_reg_imm64(code, REG_RAX, 12);
    emit_syscall(code);

    emit_mov_reg_imm64(code, REG_RSI, kiln_heap_limit_addr());
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RDI); // heap_limit = base + one chunk

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

void heap_emit_checkpoint(CodeBuf *code) {
    emit_mov_reg_imm64(code, REG_RSI, kiln_heap_ptr_addr());
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0);
}

void heap_emit_reset(CodeBuf *code) {
    emit_mov_reg_imm64(code, REG_RSI, kiln_heap_ptr_addr());
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RDI);
}
