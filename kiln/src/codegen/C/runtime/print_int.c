#include <string.h>
#include "codegen/H/runtime/print_int.h"
#include "codegen/H/runtime/number_format.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/emit/layout.h"
#include "codegen/H/platform/win_call.h"
#include "codegen/H/platform/platform_console.h"
#include "elf/H/elf_dynamic_call.h"
#include "app/H/target.h"

// Embeds a fixed byte sequence directly in the code stream (jumped over
// at runtime, never executed) and writes it to stdout -- used for the
// small fixed strings ("none", "<function>") that don't need per-call
// digit/content computation. Position-(in)dependent addressing matches
// the technique the NIL case already used before this file grew a
// second caller for it (see the KILN_TARGET_LINUX/so comment below).
static int emit_literal(CodeBuf *code, const char *s, int len) {
    int skip = emit_jmp_rel32(code);
    int data_offset = code->count;
    for (int i = 0; i < len; i++) emit_byte(code, (uint8_t)s[i]);
    emit_patch_jump(code, skip);
    return data_offset;
}

static void write_literal(CodeBuf *code, int data_offset, int len) {
    if (kiln_is_compiling_so() && kiln_get_target() == KILN_TARGET_LINUX) {
        emit_lea_rip_back(code, REG_RSI, data_offset);
    } else {
        uint64_t addr = kiln_code_base() + (uint64_t)data_offset;
        emit_mov_reg_imm64(code, REG_RSI, addr);
    }
    emit_mov_reg_imm64(code, REG_RDX, (uint64_t)len);
    platform_emit_write_stdout(code);
}

// Writes a single literal byte via a throwaway one-byte stack buffer --
// the same shape the original NIL/newline code used, generalized into a
// helper since the array/map printer below needs it for every bracket,
// comma, colon and quote. Clobbers RAX/RCX/RDX/RSI (and RBX/RDI on the
// Windows win_call path) like any call to platform_emit_write_stdout --
// callers that need something to survive across this keep it in memory
// ([RBP-disp], never a register), never in a register.
static void write_one_char(CodeBuf *code, char ch) {
    emit_sub_reg_imm8(code, REG_RSP, 8);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_store_byte_imm(code, REG_RCX, (uint8_t)ch);
    emit_mov_reg_reg(code, REG_RSI, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, 1);
    platform_emit_write_stdout(code);
    emit_add_reg_imm8(code, REG_RSP, 8);
}

// ---- number formatting, extracted into its own callable routine ----
//
// Entry: RAX = a double's raw bit pattern. Writes just the digits (and
// sign, and '.' + fraction digits if any) to stdout -- deliberately NO
// trailing newline, so both the top-level print routine (which adds
// exactly one newline after whichever path it took) and the array/map
// printer below (which never wants a newline mid-structure) can share
// it. This used to be inlined directly in the top-level routine's NUMBER
// path, newline and all; splitting it out is what let the array/map
// path below reuse it instead of duplicating ~90 lines of digit-building
// asm.
//
// Scratch layout, all within one `sub rsp, 40` region:
//   [rsp, rsp+32)   integer digits (+ optional '-'), built BACKWARD --
//                   RCX decrements before each store so no reversal
//                   step is needed.
//   rsp+32          '.' (only meaningful if frac_len > 0)
//   [rsp+33, rsp+39) up to 6 fraction digits, built FORWARD
// RBX holds the sign flag through the integer-digit phase, then gets
// reused as frac_len (how many of the 6 fraction digits are significant,
// i.e. up to the last nonzero one) -- safe, sign has already been
// written to the buffer by the time that phase starts.
static int number_routine_offset = -1;

static void emit_number_routine(CodeBuf *code) {
    int routine_skip = emit_jmp_rel32(code);
    number_routine_offset = code->count;

    number_format_emit(code); // RCX = text start, RDX = text end (on the stack)
    emit_mov_reg_reg(code, REG_RSI, REG_RCX);
    emit_sub_reg_reg(code, REG_RDX, REG_RCX);
    platform_emit_write_stdout(code);

    emit_add_reg_imm8(code, REG_RSP, NUMBER_FORMAT_SCRATCH);
    emit_ret(code);
    emit_patch_jump(code, routine_skip);
}

// ---- recursive array/map printer ----
//
// Entry: RAX = payload, RBX = tag -- same convention as the top-level
// print routine, so array elements and map values can be fed straight
// into it without re-tagging. Unlike the top-level routine, strings ARE
// quoted here (matching ashvm's vm_print_value: an element/value that's
// a string always prints with quotes, even though a bare top-level `say
// "hi";` doesn't) -- see value.c's vm_print_value for the shape this
// mirrors. Never writes a trailing newline; the top-level routine adds
// exactly one, once, after the whole structure has printed.
//
// Own stack frame (own RBP, not the enclosing ash function's -- pushed
// and restored like any ordinary call) holds the three things that must
// survive across both the write(2) calls AND the recursive self-calls
// below, since those clobber every GP register this module uses:
//   [RBP-8]  data_ptr (array) / entries_ptr (map)
//   [RBP-16] count
//   [RBP-24] index
// [RBP+disp] addressing (not [RSP+disp], which this codegen layer can't
// even encode -- RSP as a base needs a SIB byte modrm_mem_disp32()
// doesn't emit, see emit.c) survives any balanced call uneventfully: a
// call's net effect on RSP is always zero once it returns (push
// return-addr, callee's own matched sub/add rsp, ret pops the
// return-addr), so RBP itself -- untouched by any callee here, each of
// which is careful to push/pop its OWN RBP if it uses one at all -- keeps
// addressing the exact same bytes before and after.
static int print_recursive_offset = -1;

static void emit_print_recursive_routine(CodeBuf *code) {
    int routine_skip = emit_jmp_rel32(code);
    print_recursive_offset = code->count;

    emit_push_reg(code, REG_RBP);
    emit_mov_reg_reg(code, REG_RBP, REG_RSP);
    emit_sub_reg_imm8(code, REG_RSP, 32); // [RBP-8/-16/-24] loop state, [RBP-32] map's current entry addr

    int to_epilogue[8];
    int n_epilogue = 0;

    emit_cmp_reg_imm32(code, REG_RBX, TAG_STRING);
    int not_string = emit_jcc_rel32(code, COND_NE);
    {
        // write_one_char() clobbers RAX/RCX/RDX/RSI (it's its own call to
        // platform_emit_write_stdout), so length/buf must be saved via the
        // real stack -- NOT held in a register -- across the opening quote.
        emit_load_mem_disp32(code, REG_RDX, REG_RAX, -8); // length
        emit_mov_reg_reg(code, REG_RSI, REG_RAX);          // buf
        emit_push_reg(code, REG_RSI);
        emit_push_reg(code, REG_RDX);
        write_one_char(code, '"');
        emit_pop_reg(code, REG_RDX);
        emit_pop_reg(code, REG_RSI);
        platform_emit_write_stdout(code);
        write_one_char(code, '"');
        to_epilogue[n_epilogue++] = emit_jmp_rel32(code);
    }
    emit_patch_jump(code, not_string);

    emit_cmp_reg_imm32(code, REG_RBX, TAG_NIL);
    int not_nil = emit_jcc_rel32(code, COND_NE);
    {
        static int none_offset = -1;
        if (none_offset == -1) none_offset = emit_literal(code, "none", 4);
        write_literal(code, none_offset, 4);
        to_epilogue[n_epilogue++] = emit_jmp_rel32(code);
    }
    emit_patch_jump(code, not_nil);

    emit_cmp_reg_imm32(code, REG_RBX, TAG_NUMBER);
    int not_number = emit_jcc_rel32(code, COND_NE);
    {
        emit_call_back(code, number_routine_offset); // RAX already = payload bits
        to_epilogue[n_epilogue++] = emit_jmp_rel32(code);
    }
    emit_patch_jump(code, not_number);

    emit_cmp_reg_imm32(code, REG_RBX, TAG_BOOL);
    int not_bool = emit_jcc_rel32(code, COND_NE);
    {
        static int yes_offset = -1;
        static int no_offset = -1;
        if (yes_offset == -1) yes_offset = emit_literal(code, "yes", 3);
        if (no_offset == -1) no_offset = emit_literal(code, "no", 2);
        emit_cmp_reg_imm32(code, REG_RAX, 0); // payload bits: 0.0 is all zero
        int is_no = emit_jcc_rel32(code, COND_E);
        write_literal(code, yes_offset, 3);
        to_epilogue[n_epilogue++] = emit_jmp_rel32(code);
        emit_patch_jump(code, is_no);
        write_literal(code, no_offset, 2);
        to_epilogue[n_epilogue++] = emit_jmp_rel32(code);
    }
    emit_patch_jump(code, not_bool);

    emit_cmp_reg_imm32(code, REG_RBX, TAG_ARRAY);
    int not_array = emit_jcc_rel32(code, COND_NE);
    {
        emit_mov_reg_reg(code, REG_RCX, REG_RAX);          // array object addr
        emit_load_mem_disp32(code, REG_RDX, REG_RCX, 8);   // count
        emit_load_mem_disp32(code, REG_RSI, REG_RCX, 16);  // data_ptr
        emit_store_mem_disp32(code, REG_RBP, -16, REG_RDX);
        emit_store_mem_disp32(code, REG_RBP, -8, REG_RSI);
        emit_mov_reg_imm64(code, REG_RDX, 0);
        emit_store_mem_disp32(code, REG_RBP, -24, REG_RDX); // index = 0

        write_one_char(code, '[');

        int loop_top = code->count;
        emit_load_mem_disp32(code, REG_RCX, REG_RBP, -24); // index
        emit_load_mem_disp32(code, REG_RDX, REG_RBP, -16); // count
        emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
        int loop_end = emit_jcc_rel32(code, COND_GE);

        emit_load_mem_disp32(code, REG_RSI, REG_RBP, -8); // data_ptr
        emit_mov_reg_reg(code, REG_RDX, REG_RCX);
        emit_mov_reg_imm64(code, REG_RAX, 16);
        emit_imul_reg_reg(code, REG_RDX, REG_RAX); // index * 16
        emit_add_reg_reg(code, REG_RDX, REG_RSI);  // element addr
        emit_load_mem_disp32(code, REG_RBX, REG_RDX, 0); // elem tag
        emit_load_mem_disp32(code, REG_RAX, REG_RDX, 8); // elem payload
        emit_call_back(code, print_recursive_offset); // recursive -- quotes strings

        // index++ is stored BEFORE deciding whether to print a comma --
        // not held in a register across write_one_char(','), which
        // clobbers RCX (see write_one_char's own comment). The decision
        // itself re-reads both values fresh from the frame for the same
        // reason: nothing here may assume a register survives a call.
        emit_load_mem_disp32(code, REG_RCX, REG_RBP, -24); // index
        emit_add_reg_imm8(code, REG_RCX, 1);
        emit_store_mem_disp32(code, REG_RBP, -24, REG_RCX); // index++
        emit_load_mem_disp32(code, REG_RCX, REG_RBP, -24);
        emit_load_mem_disp32(code, REG_RDX, REG_RBP, -16); // count
        emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
        int skip_comma = emit_jcc_rel32(code, COND_GE);
        write_one_char(code, ',');
        write_one_char(code, ' ');
        emit_patch_jump(code, skip_comma);
        emit_jmp_back(code, loop_top);

        emit_patch_jump(code, loop_end);
        write_one_char(code, ']');
        to_epilogue[n_epilogue++] = emit_jmp_rel32(code);
    }
    emit_patch_jump(code, not_array);

    emit_cmp_reg_imm32(code, REG_RBX, TAG_MAP);
    int not_map = emit_jcc_rel32(code, COND_NE);
    {
        emit_mov_reg_reg(code, REG_RCX, REG_RAX);          // map object addr
        emit_load_mem_disp32(code, REG_RDX, REG_RCX, 8);   // count
        emit_load_mem_disp32(code, REG_RSI, REG_RCX, 16);  // entries_ptr
        emit_store_mem_disp32(code, REG_RBP, -16, REG_RDX);
        emit_store_mem_disp32(code, REG_RBP, -8, REG_RSI);
        emit_mov_reg_imm64(code, REG_RDX, 0);
        emit_store_mem_disp32(code, REG_RBP, -24, REG_RDX); // index = 0

        write_one_char(code, '{');

        int loop_top = code->count;
        emit_load_mem_disp32(code, REG_RCX, REG_RBP, -24); // index
        emit_load_mem_disp32(code, REG_RDX, REG_RBP, -16); // count
        emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
        int loop_end = emit_jcc_rel32(code, COND_GE);

        emit_load_mem_disp32(code, REG_RSI, REG_RBP, -8); // entries_ptr
        emit_mov_reg_reg(code, REG_RDX, REG_RCX);
        emit_mov_reg_imm64(code, REG_RAX, 32);
        emit_imul_reg_reg(code, REG_RDX, REG_RAX); // index * 32
        emit_add_reg_reg(code, REG_RDX, REG_RSI);  // entry addr
        // Persisted in the frame, not a register -- it has to survive the
        // key write, the value's tag/payload load, AND the recursive
        // call below, and every one of those clobbers RAX/RCX/RDX/RSI.
        emit_store_mem_disp32(code, REG_RBP, -32, REG_RDX);

        // key: always a string (value.h) -- quoted, written directly
        // rather than through the recursive call (no tag dispatch needed).
        write_one_char(code, '"');
        emit_load_mem_disp32(code, REG_RCX, REG_RBP, -32); // entry addr
        emit_load_mem_disp32(code, REG_RAX, REG_RCX, 8);   // key payload
        emit_load_mem_disp32(code, REG_RDX, REG_RAX, -8);  // key length
        emit_mov_reg_reg(code, REG_RSI, REG_RAX);           // key buf
        platform_emit_write_stdout(code);
        write_one_char(code, '"');
        write_one_char(code, ':');
        write_one_char(code, ' ');

        emit_load_mem_disp32(code, REG_RCX, REG_RBP, -32); // entry addr
        emit_load_mem_disp32(code, REG_RBX, REG_RCX, 16);  // value tag
        emit_load_mem_disp32(code, REG_RAX, REG_RCX, 24);  // value payload
        emit_call_back(code, print_recursive_offset); // recursive -- quotes strings

        // See the array loop's matching comment: store index++ before
        // deciding on a comma, then re-read both values fresh.
        emit_load_mem_disp32(code, REG_RCX, REG_RBP, -24); // index
        emit_add_reg_imm8(code, REG_RCX, 1);
        emit_store_mem_disp32(code, REG_RBP, -24, REG_RCX); // index++
        emit_load_mem_disp32(code, REG_RCX, REG_RBP, -24);
        emit_load_mem_disp32(code, REG_RDX, REG_RBP, -16); // count
        emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
        int skip_comma = emit_jcc_rel32(code, COND_GE);
        write_one_char(code, ',');
        write_one_char(code, ' ');
        emit_patch_jump(code, skip_comma);
        emit_jmp_back(code, loop_top);

        emit_patch_jump(code, loop_end);
        write_one_char(code, '}');
        to_epilogue[n_epilogue++] = emit_jmp_rel32(code);
    }
    emit_patch_jump(code, not_map);

    // Fallback (TAG_FUNCTION, or anything else): matches ashvm's
    // "<function>"/"<closure>" rather than silently misprinting the
    // function object's code address as a number.
    {
        static int function_offset = -1;
        if (function_offset == -1) function_offset = emit_literal(code, "<function>", 10);
        write_literal(code, function_offset, 10);
        // falls straight through to the epilogue below
    }

    for (int i = 0; i < n_epilogue; i++) emit_patch_jump(code, to_epilogue[i]);
    emit_mov_reg_reg(code, REG_RSP, REG_RBP);
    emit_pop_reg(code, REG_RBP);
    emit_ret(code);
    emit_patch_jump(code, routine_skip);
}

// Dispatches on the popped value's tag: a STRING just gets written
// directly (it's already bytes -- no digit conversion needed at all,
// unlike a number, and unquoted here -- only array/map elements get
// quoted, see emit_print_recursive_routine above); NUMBER goes to
// number_routine; everything else (ARRAY, MAP, FUNCTION) delegates to
// the recursive printer. Each path ends with exactly one trailing
// newline, written once after the value's own content, then control
// converges on a single `ret`.
//
// Plan B, phase B1: emitted once (jumped over), called via emit_call_back
// from codegen_print_top below -- this is the single biggest deduplication
// win of B1, since this routine used to be re-emitted in full at EVERY
// print statement in the source (dwarfing heap_emit_alloc/bytes_emit_copy,
// which are individually smaller). Takes its value in RAX=payload,
// RBX=tag (NOT by popping them itself): a real CALL pushes a return
// address on top of whatever the caller already pushed, so a `pop` here
// would grab the return address instead of the caller's value -- same
// register-based-argument reasoning codegen/C/runtime/heap.c's alloc routine
// already uses, not something print specifically needed before this
// became a real subroutine.
static int print_routine_offset = -1;

static void emit_print_routine(CodeBuf *code) {
    int routine_skip = emit_jmp_rel32(code);
    print_routine_offset = code->count;
    emit_cmp_reg_imm32(code, REG_RBX, TAG_STRING);
    int is_number = emit_jcc_rel32(code, COND_NE);

    // ---- STRING path: [payload-8] = length, [payload..] = bytes ----
    emit_load_mem_disp32(code, REG_RDX, REG_RAX, -8); // length
    emit_mov_reg_reg(code, REG_RSI, REG_RAX);          // buf
    platform_emit_write_stdout(code);
    write_one_char(code, '\n');
    int done = emit_jmp_rel32(code);
    emit_patch_jump(code, is_number);

    // ---- NIL path ----
    emit_cmp_reg_imm32(code, REG_RBX, TAG_NIL);
    int not_nil = emit_jcc_rel32(code, COND_NE);
    {
        int skip = emit_jmp_rel32(code);
        int data_offset = code->count;
        emit_byte(code, 'n'); emit_byte(code, 'o'); emit_byte(code, 'n'); emit_byte(code, 'e');
        emit_patch_jump(code, skip);
        write_literal(code, data_offset, 4);
        write_one_char(code, '\n');
    }
    int done_nil = emit_jmp_rel32(code);
    emit_patch_jump(code, not_nil);

    // ---- NUMBER path ----
    emit_cmp_reg_imm32(code, REG_RBX, TAG_NUMBER);
    int not_number = emit_jcc_rel32(code, COND_NE);
    {
        emit_call_back(code, number_routine_offset); // RAX already = payload bits
        write_one_char(code, '\n');
    }
    int done_number = emit_jmp_rel32(code);
    emit_patch_jump(code, not_number);

    // ---- everything else (ARRAY, MAP, FUNCTION) -- delegate to the
    // recursive printer, which already has a case for each (and a
    // "<function>" fallback for anything further afield than that) ----
    emit_call_back(code, print_recursive_offset);
    write_one_char(code, '\n');

    emit_patch_jump(code, done);
    emit_patch_jump(code, done_nil);
    emit_patch_jump(code, done_number);
    emit_ret(code);
    emit_patch_jump(code, routine_skip);
}

void print_emit_top_routine_only(CodeBuf *code) {
    emit_number_routine(code);
    emit_print_recursive_routine(code);
    emit_print_routine(code);
}

int print_top_routine_offset(void) { return print_routine_offset; }

static int is_shared(void) {
    return kiln_get_link_mode() == KILN_LINK_SHARED && !kiln_is_compiling_so();
}

void print_emit_startup(CodeBuf *code) {
    if (!is_shared()) {
        emit_number_routine(code);
        emit_print_recursive_routine(code);
        emit_print_routine(code);
    }
}

void codegen_print_top(CodeBuf *code) {
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    if (is_shared()) {
        if (kiln_get_target() == KILN_TARGET_WINDOWS) win_call_runtime_import(code, RUNTIME_IMPORT_PRINT_TOP);
        else elf_dynamic_call(code, RUNTIME_IMPORT_PRINT_TOP);
        return;
    }
    emit_call_back(code, print_routine_offset);
}

void codegen_exit0(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        win_call_begin(code, 0);
        win_call_arg_imm64(code, 0, 0); // ExitProcess(0)
        win_call_import(code, PE_IMPORT_EXIT_PROCESS);
        return; // never returns -- no win_call_end needed
    }
    emit_mov_reg_imm64(code, REG_RDI, 0);
    emit_mov_reg_imm64(code, REG_RAX, 60); // syscall: exit
    emit_syscall(code);
}
