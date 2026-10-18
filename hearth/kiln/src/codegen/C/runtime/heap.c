#include "codegen/H/runtime/heap.h"
#include "codegen/H/emit/layout.h"
#include "codegen/H/platform/win_call.h"
#include "elf/H/elf_dynamic_call.h"
#include "app/H/target.h"
#include "codegen/H/emit/value.h"

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
// ---- optional free lists (static Linux builds of programs that free; see heap.h) ----
#define FL_MAX_BLOCK 512        // blocks up to this size are recycled, larger ones are only bump-allocated
#define FL_ROUND_MASK (~(uint64_t)15)

static int free_lists_on;
static int arrays_on;

void heap_enable_free_lists(int blocks) {
    free_lists_on = kiln_get_link_mode() == KILN_LINK_STATIC && !kiln_is_compiling_so();   // Linux and Windows static builds
    arrays_on = free_lists_on && blocks;
}

int heap_free_arrays_on(void) { return arrays_on; }

int heap_free_lists_on(void) { return free_lists_on; }

// Start of the string alloc routine: rounds the request up to 16 bytes (every string block is then a whole
// size class, so a freed one can serve any request of its class) and pops a recycled block of that class if
// there is one; otherwise falls through with the rounded size in RDI.
// In: RDI = size. Out: RAX = block (recycled path only). Clobbers RSI, RDI like the alloc routine.
static void emit_freelist_pop(CodeBuf *code) {
    emit_add_reg_imm8(code, REG_RDI, 15);
    emit_mov_reg_imm64(code, REG_RSI, FL_ROUND_MASK);
    emit_and_reg_reg(code, REG_RDI, REG_RSI);
    emit_cmp_reg_imm32(code, REG_RDI, FL_MAX_BLOCK);
    int too_big = emit_jcc_rel32(code, COND_A);
    emit_mov_reg_reg(code, REG_RSI, REG_RDI);
    emit_sar_reg_by1(code, REG_RSI);                              // size / 2 = class * 8
    emit_mov_reg_imm64(code, REG_RAX, kiln_freelist_addr());
    emit_add_reg_reg(code, REG_RSI, REG_RAX);                     // RSI = &head of this class
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0);              // RAX = head
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int empty = emit_jcc_rel32(code, COND_E);
    emit_load_mem_disp32(code, REG_RDI, REG_RAX, 0);              // RDI = next free block
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RDI);             // head = next
    emit_ret(code);
    emit_patch_jump(code, empty);
    emit_patch_jump(code, too_big);
}

// Puts the block starting at RAX, `size` bytes (RDX, a multiple of 16) on the free list of its class.
// Does nothing for blocks above FL_MAX_BLOCK. Clobbers RAX, RCX, RDX, RSI.
static void emit_free_block(CodeBuf *code) {
    emit_cmp_reg_imm32(code, REG_RDX, FL_MAX_BLOCK);
    int done = emit_jcc_rel32(code, COND_A);
    emit_cmp_reg_imm32(code, REG_RDX, 16);
    int tiny = emit_jcc_rel32(code, COND_B);
    emit_mov_reg_reg(code, REG_RSI, REG_RDX);
    emit_sar_reg_by1(code, REG_RSI);
    emit_mov_reg_imm64(code, REG_RCX, kiln_freelist_addr());
    emit_add_reg_reg(code, REG_RSI, REG_RCX);                     // RSI = &head
    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 0);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);             // block.next = head
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RAX);             // head = block
    emit_patch_jump(code, tiny);
    emit_patch_jump(code, done);
}

// A string payload in RAX (a heap block made by the concatenation or str() code, whose 8-byte length
// prefix sits right before it) goes back on the free list of its class. The class is computed from the
// length; the block is at least that big, so reusing it for that class is always in bounds.
// Clobbers RAX, RCX, RDX, RSI. Does nothing for blocks above FL_MAX_BLOCK.
void heap_emit_free_string_rax(CodeBuf *code) {
    emit_load_mem_disp32(code, REG_RDX, REG_RAX, -8);             // length
    emit_add_reg_imm8(code, REG_RDX, 8 + 15);
    emit_mov_reg_imm64(code, REG_RSI, FL_ROUND_MASK);
    emit_and_reg_reg(code, REG_RDX, REG_RSI);                     // size class
    emit_sub_reg_imm8(code, REG_RAX, 8);                          // block start
    emit_free_block(code);
}

// RAX = an array object made by the array literal / mem_newarr1 code while the array free lists were on
// (its 24 bytes were allocated as a 32-byte class block); its element block is capacity * 16 bytes (16 for
// capacity 0). Every array block is allocated through heap_emit_alloc_recyclable, so both are whole classes.
// Both go back on the free lists. Clobbers RAX, RCX, RDX, RSI.
void heap_emit_free_array_rax(CodeBuf *code) {
    emit_push_reg(code, REG_RAX);
    emit_load_mem_disp32(code, REG_RDX, REG_RAX, 0);              // capacity
    emit_mov_reg_imm64(code, REG_RCX, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RCX);                    // element block size
    emit_cmp_reg_imm32(code, REG_RDX, 0);
    int sized = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RDX, 16);                        // an empty array's 1-byte block was rounded up to 16
    emit_patch_jump(code, sized);
    emit_load_mem_disp32(code, REG_RAX, REG_RAX, 16);             // data_ptr
    emit_free_block(code);
    emit_pop_reg(code, REG_RAX);                                  // the object itself
    emit_mov_reg_imm64(code, REG_RDX, 32);
    emit_free_block(code);
}

// RAX = the 16-byte block an empty map literal / array literal was given (a 1-byte request rounded up);
// puts it back. Clobbers RAX, RCX, RDX, RSI.
void heap_emit_free_empty_block_rax(CodeBuf *code) {
    emit_mov_reg_imm64(code, REG_RDX, 16);
    emit_free_block(code);
}

// RAX = a map object made by the map literal code while the free lists for blocks were on (an object of a
// plain class is one: its fields are all in the literal, so it never grows); entries block = capacity * 32
// bytes (16 for capacity 0). Both go back on the free lists. Clobbers RAX, RCX, RDX, RSI.
void heap_emit_free_map_rax(CodeBuf *code) {
    emit_push_reg(code, REG_RAX);
    emit_load_mem_disp32(code, REG_RDX, REG_RAX, 0);              // capacity
    emit_mov_reg_imm64(code, REG_RCX, 32);
    emit_imul_reg_reg(code, REG_RDX, REG_RCX);
    emit_cmp_reg_imm32(code, REG_RDX, 0);
    int sized = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RDX, 16);
    emit_patch_jump(code, sized);
    emit_load_mem_disp32(code, REG_RAX, REG_RAX, 16);             // entries_ptr
    emit_free_block(code);
    emit_pop_reg(code, REG_RAX);
    emit_mov_reg_imm64(code, REG_RDX, 32);
    emit_free_block(code);
}

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

// Only strings are ever freed, so only their allocations (concatenation, str()) go through this routine;
// arrays, maps and everything else keep calling the plain bump routine, which is byte-for-byte the old one.
static int str_alloc_offset = -1;

static void emit_str_alloc_routine(CodeBuf *code) {
    int skip = emit_jmp_rel32(code);
    str_alloc_offset = code->count;
    emit_freelist_pop(code);
    emit_call_back(code, alloc_routine_offset);   // nothing to recycle: bump-allocate the rounded size
    emit_ret(code);
    emit_patch_jump(code, skip);
}

void heap_emit_alloc_recyclable(CodeBuf *code) {
    if (arrays_on && str_alloc_offset >= 0) emit_call_back(code, str_alloc_offset);
    else heap_emit_alloc(code);
}

void heap_emit_alloc_str(CodeBuf *code) {
    if (free_lists_on && str_alloc_offset >= 0) emit_call_back(code, str_alloc_offset);
    else heap_emit_alloc(code);
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

    if (!is_shared()) {
        emit_alloc_routine(code);
        if (free_lists_on) emit_str_alloc_routine(code);
    }
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
