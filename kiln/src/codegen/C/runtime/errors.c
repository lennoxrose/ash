#include <string.h>
#include "codegen/H/runtime/errors.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/emit/layout.h"
#include "codegen/H/emit/runtime_layout.h"
#include "codegen/H/platform/platform_console.h"
#include "codegen/H/platform/win_call.h"
#include "elf/H/elf_dynamic_call.h"
#include "app/H/target.h"

static int raise_routine_offset = -1;

// Writes `text` (<= 31 bytes) to stderr via a stack buffer. Clobbers
// RAX/RCX/RDX/RSI (and RBX/RDI on Windows), like platform_emit_write_stderr.
static void write_stderr_text(CodeBuf *code, const char *text) {
    int n = (int)strlen(text);
    emit_sub_reg_imm8(code, REG_RSP, 32);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    for (int i = 0; i < n; i++) {
        emit_store_byte_imm(code, REG_RCX, (uint8_t)text[i]);
        emit_add_reg_imm8(code, REG_RCX, 1);
    }
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_mov_reg_imm64(code, REG_RDX, (uint64_t)n);
    platform_emit_write_stderr(code);
    emit_add_reg_imm8(code, REG_RSP, 32);
}

static int is_shared(void) {
    return kiln_get_link_mode() == KILN_LINK_SHARED && !kiln_is_compiling_so();
}

// Embeds `msg` as a proper length-prefixed string (jumped over, never
// executed as instructions) -- the SAME layout codegen/C/strings/strings.c's
// literals use ([payload-8]=length, no trailing newline in the data
// itself), since milestone 9 makes a caught error a real, usable
// TAG_STRING value (print/concat/equality all assume this layout). This
// embedding always happens in the EXECUTABLE (call sites for
// errors_emit_die/fatal live all over the codebase, never in the
// extracted runtime), so kiln_code_base() -- which only knows a fixed
// EXECUTABLE's own base -- is always the right address to use here, even
// under --link=shared.
static void embed_message(CodeBuf *code, const char *msg, uint64_t *out_addr, int *out_len) {
    int len = (int)strlen(msg);
    int skip = emit_jmp_rel32(code);
    int data_offset = code->count;
    emit_u64(code, (uint64_t)len);
    for (int i = 0; i < len; i++) emit_byte(code, (uint8_t)msg[i]);
    emit_patch_jump(code, skip);
    *out_addr = kiln_code_base() + (uint64_t)data_offset + 8;
    *out_len = len;
}

// Raise routine -- inputs RBX=value tag, RSI=value payload, RDX=message len
// (only meaningful when the tag is STRING). If an `attempt`
// is active (kiln_try_depth_addr() > 0), pops the innermost handler,
// restores its saved RSP/RBP (kiln's hand-rolled longjmp -- no libc
// setjmp in a freestanding binary), stores the message as a TAG_STRING
// value into the handler's recorded error-variable slot, and jumps to
// its handle block. Otherwise falls through to an inline fatal path
// (write to stderr, exit 1).
static void emit_raise_routine(CodeBuf *code) {
    int skip = emit_jmp_rel32(code);
    raise_routine_offset = code->count;

    emit_push_reg(code, REG_RBX); // the raised value's tag, kept until the handler's slot is known
    emit_mov_reg_imm64(code, REG_RAX, kiln_try_depth_addr());
    emit_load_mem_disp32(code, REG_RAX, REG_RAX, 0);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int go_fatal = emit_jcc_rel32(code, COND_E);

    emit_dec_reg(code, REG_RAX);
    emit_mov_reg_imm64(code, REG_RCX, kiln_try_depth_addr());
    emit_store_mem_disp32(code, REG_RCX, 0, REG_RAX); // pop handler

    emit_mov_reg_imm64(code, REG_RCX, KILN_TRY_HANDLER_SIZE);
    emit_imul_reg_reg(code, REG_RAX, REG_RCX); // RAX = index * handler size
    emit_mov_reg_imm64(code, REG_RDI, kiln_try_handlers_addr());
    emit_add_reg_reg(code, REG_RAX, REG_RDI); // RAX = handler slot address

    emit_load_mem_disp32(code, REG_RCX, REG_RAX, 0);  // saved rsp
    emit_load_mem_disp32(code, REG_RDI, REG_RAX, 8);  // saved rbp
    emit_load_mem_disp32(code, REG_RDX, REG_RAX, 16); // handle target (clobbers RDX -- fine, msg len only needed on the fatal branch we didn't take)
    emit_load_mem_disp32(code, REG_RBX, REG_RAX, 24); // error variable's rbp-relative tag offset

    emit_pop_reg(code, REG_RAX); // the raised value's tag
    emit_mov_reg_reg(code, REG_RSP, REG_RCX);
    emit_mov_reg_reg(code, REG_RBP, REG_RDI);

    emit_mov_reg_reg(code, REG_RCX, REG_RBP);
    emit_add_reg_reg(code, REG_RCX, REG_RBX); // RCX = error variable's address
    emit_store_mem_disp32(code, REG_RCX, 0, REG_RAX); // tag
    emit_store_mem_disp32(code, REG_RCX, 8, REG_RSI); // payload = message address

    emit_jmp_indirect(code, REG_RDX);

    emit_patch_jump(code, go_fatal);
    // RSI/RDX still hold this call's original message addr/len -- nothing
    // between the raise routine's entry and here touches them on the path
    // that skips straight past the attempt-active handling.
    emit_pop_reg(code, REG_RAX); // tag
    emit_push_reg(code, REG_RSI);
    emit_push_reg(code, REG_RDX);
    emit_push_reg(code, REG_RAX);
    write_stderr_text(code, "error: ");
    emit_pop_reg(code, REG_RAX);
    emit_pop_reg(code, REG_RDX);
    emit_pop_reg(code, REG_RSI);
    emit_cmp_reg_imm32(code, REG_RAX, TAG_STRING);
    int is_text = emit_jcc_rel32(code, COND_E);
    write_stderr_text(code, "uncaught non-string value");
    int text_done = emit_jmp_rel32(code);
    emit_patch_jump(code, is_text);
    platform_emit_write_stderr(code);
    emit_patch_jump(code, text_done);
    emit_sub_reg_imm8(code, REG_RSP, 8);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_store_byte_imm(code, REG_RCX, '\n');
    emit_mov_reg_reg(code, REG_RSI, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, 1);
    platform_emit_write_stderr(code);
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        win_call_begin(code, 0);
        win_call_arg_imm64(code, 0, 1); // ExitProcess(1)
        win_call_import(code, PE_IMPORT_EXIT_PROCESS);
    } else {
        emit_mov_reg_imm64(code, REG_RDI, 1); // exit status 1
        emit_mov_reg_imm64(code, REG_RAX, 60); // syscall: exit
        emit_syscall(code);
    }

    emit_patch_jump(code, skip);
}

void errors_emit_raise_routine_only(CodeBuf *code) {
    emit_raise_routine(code);
}

int errors_raise_routine_offset(void) { return raise_routine_offset; }

void errors_emit_startup(CodeBuf *code) {
    if (!is_shared()) emit_raise_routine(code);
}

static void emit_die(CodeBuf *code) {
    if (is_shared()) {
        if (kiln_get_target() == KILN_TARGET_WINDOWS) win_call_runtime_import(code, RUNTIME_IMPORT_RAISE);
        else elf_dynamic_call(code, RUNTIME_IMPORT_RAISE);
        return;
    }
    emit_jmp_back(code, raise_routine_offset); // never returns -- no need to come back
}

void errors_emit_die(CodeBuf *code, const char *msg) {
    // Call sites spell messages "runtime error: ..."; the caught value is just
    // the text after that (identical to ashvm's), and the fatal path adds its own
    // "error: " prefix.
    static const char prefix[] = "runtime error: ";
    if (strncmp(msg, prefix, sizeof(prefix) - 1) == 0) msg += sizeof(prefix) - 1;
    uint64_t addr; int len;
    embed_message(code, msg, &addr, &len);
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_mov_reg_imm64(code, REG_RSI, addr);
    emit_mov_reg_imm64(code, REG_RDX, (uint64_t)len);
    emit_die(code);
}

// The ONE case that must never be "catchable" (exceeding
// MAX_KILN_TRY_DEPTH itself -- silently continuing there would mean
// pushing a handler past the fixed table, corrupting the adjacent code
// segment). Only ever called from one site (parser/C/statements/attempt_handle.c), so
// there's no deduplication payoff in routing it through the shared
// runtime at all -- it just emits its own small inline write+exit
// sequence directly, in every link mode, decoupled from wherever RAISE
// itself happens to live.
void errors_emit_fatal(CodeBuf *code, const char *msg) {
    uint64_t addr; int len;
    embed_message(code, msg, &addr, &len);
    emit_mov_reg_imm64(code, REG_RSI, addr);
    emit_mov_reg_imm64(code, REG_RDX, (uint64_t)len);
    platform_emit_write_stderr(code);
    emit_sub_reg_imm8(code, REG_RSP, 8);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_store_byte_imm(code, REG_RCX, '\n');
    emit_mov_reg_reg(code, REG_RSI, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, 1);
    platform_emit_write_stderr(code);
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        win_call_begin(code, 0);
        win_call_arg_imm64(code, 0, 1);
        win_call_import(code, PE_IMPORT_EXIT_PROCESS);
    } else {
        emit_mov_reg_imm64(code, REG_RDI, 1);
        emit_mov_reg_imm64(code, REG_RAX, 60);
        emit_syscall(code);
    }
}

void errors_emit_die_dynamic(CodeBuf *code) {
    emit_die(code);
}
