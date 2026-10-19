#include <stdlib.h>
#include <string.h>
#include "codegen/H/runtime/trace.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/layout.h"
#include "codegen/H/emit/runtime_layout.h"
#include "codegen/H/emit/value.h"
#include "app/H/target.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/strings/strings.h"
#include "parser/H/core/parser.h"

typedef struct {
    uint64_t offset;      // code offset where the statement starts
    int line;             // 0: end of a function (no name, no line)
    int name;             // index into names[], -1 for none
} Note;

typedef struct { const char *text; int len; } Name;

static Note *notes;
static int note_count, note_cap;
static Name *names;
static int name_count, name_cap;
static int current_name = -1;
static int *patches;                // imm64 positions that must receive the table address
static int patch_count, patch_cap;

void trace_reset(void) {
    note_count = name_count = patch_count = 0;
    current_name = -1;
}

static int name_index(const char *name, int len) {
    for (int i = 0; i < name_count; i++) if (names[i].len == len && !memcmp(names[i].text, name, (size_t)len)) return i;
    if (name_count == name_cap) {
        name_cap = name_cap ? name_cap * 2 : 64;
        names = realloc(names, sizeof(Name) * (size_t)name_cap);
        if (!names) exit(1);
    }
    names[name_count] = (Name){name, len};
    return name_count++;
}

static void add_note(uint64_t offset, int line, int name) {
    if (note_count == note_cap) {
        note_cap = note_cap ? note_cap * 2 : 256;
        notes = realloc(notes, sizeof(Note) * (size_t)note_cap);
        if (!notes) exit(1);
    }
    notes[note_count++] = (Note){offset, line, name};
}

void trace_note(int code_offset, int line, const char *name, int name_len) {
    if (kiln_trace_addr() == 0) return;
    if (name) current_name = name_index(name, name_len);
    if (current_name < 0) return;
    add_note((uint64_t)code_offset, line, current_name);
}

void trace_note_end(int code_offset) {
    if (kiln_trace_addr() == 0) return;
    if (current_name >= 0) add_note((uint64_t)code_offset, 0, -1);
    current_name = -1;
}

// ---- the appended table ----------------------------------------------------------------------------------
// [u64 0]                                  the empty String: its length, the payload (the first entry) follows
// entries, 24 bytes each: {address, line, name payload address (0: none)}, ascending, ended by {INT64_MAX, 0, 0}
// the names, each as a String block: u64 length, bytes

static void put_u64(CodeBuf *c, uint64_t v) { emit_u64(c, v); }

void trace_finish(CodeBuf *code) {
    if (kiln_trace_addr() == 0 || patch_count == 0) return;     // nothing reads the table
    uint64_t base = kiln_code_base();
    int start = code->count;
    uint64_t table = base + (uint64_t)start + 8;
    put_u64(code, 0);
    int entries = note_count + 1;
    uint64_t names_at = table + 24u * (uint64_t)entries;
    uint64_t *name_addr = calloc((size_t)(name_count ? name_count : 1), sizeof(uint64_t));
    uint64_t at = names_at;
    for (int i = 0; i < name_count; i++) {
        name_addr[i] = at + 8;
        at += 8 + (uint64_t)names[i].len;
    }
    for (int i = 0; i < note_count; i++) {
        put_u64(code, base + notes[i].offset);
        put_u64(code, (uint64_t)notes[i].line);
        put_u64(code, notes[i].name >= 0 ? name_addr[notes[i].name] : 0);
    }
    put_u64(code, 0x7FFFFFFFFFFFFFFFull);
    put_u64(code, 0);
    put_u64(code, 0);
    for (int i = 0; i < name_count; i++) {
        put_u64(code, (uint64_t)names[i].len);
        for (int k = 0; k < names[i].len; k++) emit_byte(code, (uint8_t)names[i].text[k]);
    }
    for (int i = 0; i < patch_count; i++) emit_patch_imm64(code, patches[i], table);
    free(name_addr);
}

static int table_ref(CodeBuf *code, Reg dst) {
    int p = emit_mov_reg_imm64_patchable(code, dst);
    if (patch_count == patch_cap) {
        patch_cap = patch_cap ? patch_cap * 2 : 8;
        patches = realloc(patches, sizeof(int) * (size_t)patch_cap);
        if (!patches) exit(1);
    }
    patches[patch_count++] = p;
    return p;
}

// ---- run time ---------------------------------------------------------------------------------------------

void trace_emit_main_frame(CodeBuf *code) {
    uint64_t a = kiln_trace_addr();
    if (!a) return;
    emit_mov_reg_imm64(code, REG_RSI, a + 8);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RBP);
}

// Inside the raise routine, after "depth != 0": when depth == 1 only the program's own top-level handler is
// active, so this raise ends the program: remember where it happened. Raises that a user `catch` or `finally` sees
// first are not captured (they may well be caught, and a capture walks the whole stack); one that is rethrown to the
// top level is captured there. RAX (the
// depth) and RBX / RSI / RDX (the raised value) are preserved; RDI is the address of the raise site.
void trace_emit_capture(CodeBuf *code) {
    uint64_t a = kiln_trace_addr();
    if (!a) return;
    emit_cmp_reg_imm32(code, REG_RAX, 1);
    int skip = emit_jcc_rel32(code, COND_NE);
    emit_push_reg(code, REG_RAX);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RSI);
    emit_push_reg(code, REG_RDX);
    emit_mov_reg_imm64(code, REG_RCX, a);
    emit_store_mem_disp32(code, REG_RCX, 16, REG_RDI);          // ips[0]: the raise site itself
    emit_mov_reg_imm64(code, REG_RDX, 1);                        // n
    emit_mov_reg_reg(code, REG_RBX, REG_RBP);                    // the frame being walked
    emit_mov_reg_reg(code, REG_RSI, REG_RCX);
    emit_add_reg_imm8(code, REG_RSI, 8);                         // where ips[n] goes, minus the 16 byte header
    int loop = code->count;
    emit_cmp_reg_imm32(code, REG_RDX, KILN_TRACE_MAX);
    int full = emit_jcc_rel32(code, COND_GE);
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 8);             // the top-level frame
    emit_cmp_reg_reg(code, REG_RBX, REG_RAX);
    int at_top = emit_jcc_rel32(code, COND_E);
    emit_cmp_reg_imm32(code, REG_RBX, 0);
    int no_frame = emit_jcc_rel32(code, COND_E);
    emit_load_mem_disp32(code, REG_RAX, REG_RBX, 8);             // return address into the caller
    emit_store_mem_disp32(code, REG_RSI, 16, REG_RAX);           // ips[n]
    emit_add_reg_imm8(code, REG_RSI, 8);
    emit_add_reg_imm8(code, REG_RDX, 1);
    emit_load_mem_disp32(code, REG_RBX, REG_RBX, 0);             // the caller's frame
    emit_jmp_back(code, loop);
    emit_patch_jump(code, full);
    emit_patch_jump(code, at_top);
    emit_patch_jump(code, no_frame);
    emit_store_mem_disp32(code, REG_RCX, 0, REG_RDX);            // count
    emit_pop_reg(code, REG_RDX);
    emit_pop_reg(code, REG_RSI);
    emit_pop_reg(code, REG_RBX);
    emit_pop_reg(code, REG_RAX);
    emit_patch_jump(code, skip);
}

// Parses the index argument and leaves it in RDI; returns after the closing ')'.
static void index_arg(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RAX);                      // payload
    emit_pop_reg(code, REG_RBX);                      // tag
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RDI, XMM0);
}

// RDI = frame index. Leaves RCX = the table entry of that frame, or 0 (no such frame / not project code).
// Clobbers RAX, RDX, RSI, RBX.
static void lookup(CodeBuf *code, uint64_t a) {
    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_mov_reg_imm64(code, REG_RSI, a);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0);              // count
    emit_cmp_reg_reg(code, REG_RDI, REG_RAX);
    int out = emit_jcc_rel32(code, COND_GE);
    emit_cmp_reg_imm32(code, REG_RDI, 0);
    int neg = emit_jcc_rel32(code, COND_LT);
    emit_mov_reg_reg(code, REG_RAX, REG_RDI);
    emit_add_reg_reg(code, REG_RAX, REG_RAX);
    emit_add_reg_reg(code, REG_RAX, REG_RAX);
    emit_add_reg_reg(code, REG_RAX, REG_RAX);
    emit_add_reg_reg(code, REG_RAX, REG_RSI);
    emit_load_mem_disp32(code, REG_RAX, REG_RAX, 16);             // the address
    emit_cmp_reg_imm32(code, REG_RDI, 0);
    int top = emit_jcc_rel32(code, COND_E);
    emit_sub_reg_imm8(code, REG_RAX, 1);                           // a return address: inside the call, not after it
    emit_patch_jump(code, top);
    table_ref(code, REG_RDX);
    int loop = code->count;
    emit_load_mem_disp32(code, REG_RBX, REG_RDX, 0);
    emit_cmp_reg_reg(code, REG_RBX, REG_RAX);
    int done = emit_jcc_rel32(code, COND_GT);
    emit_mov_reg_reg(code, REG_RCX, REG_RDX);
    emit_add_reg_imm8(code, REG_RDX, 24);
    emit_jmp_back(code, loop);
    emit_patch_jump(code, done);
    emit_patch_jump(code, out);
    emit_patch_jump(code, neg);
}

void codegen_builtin_trace_n(void) {
    expect(TOKEN_RPAREN, "expected ')' after 'trace_n('");
    uint64_t a = kiln_trace_addr();
    if (a) {
        emit_mov_reg_imm64(code, REG_RSI, a);
        emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0);
        emit_cvtsi2sd(code, XMM0, REG_RAX);
    } else {
        emit_pxor_xmm_xmm(code, XMM0);
    }
    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

void codegen_builtin_trace_fn(void) {
    index_arg();
    uint64_t a = kiln_trace_addr();
    if (!a) {                                                      // --link=shared: no trace, no names
        codegen_string_literal_bytes("", 0);
        return;
    }
    lookup(code, a);
    table_ref(code, REG_RAX);                                      // "" unless the entry has a name
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int none = emit_jcc_rel32(code, COND_E);
    emit_load_mem_disp32(code, REG_RDX, REG_RCX, 16);
    emit_cmp_reg_imm32(code, REG_RDX, 0);
    int unnamed = emit_jcc_rel32(code, COND_E);
    emit_mov_reg_reg(code, REG_RAX, REG_RDX);
    emit_patch_jump(code, unnamed);
    emit_patch_jump(code, none);
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

void codegen_builtin_trace_line(void) {
    index_arg();
    uint64_t a = kiln_trace_addr();
    if (!a) {
        emit_pxor_xmm_xmm(code, XMM0);
    } else {
        lookup(code, a);
        emit_mov_reg_imm64(code, REG_RAX, 0);
        emit_cmp_reg_imm32(code, REG_RCX, 0);
        int none = emit_jcc_rel32(code, COND_E);
        emit_load_mem_disp32(code, REG_RAX, REG_RCX, 8);
        emit_patch_jump(code, none);
        emit_cvtsi2sd(code, XMM0, REG_RAX);
    }
    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
