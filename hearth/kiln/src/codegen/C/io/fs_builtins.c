#include <stdint.h>
#include "codegen/H/io/file_builtins.h"
#include "codegen/H/io/file_path.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/emit/bytes.h"
#include "codegen/H/collections/arrays.h"
#include "codegen/H/platform/platform_file.h"
#include "codegen/H/runtime/errors.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/strings/string_alloc.h"
#include "app/H/target.h"
#include "codegen/H/platform/win_call.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"

// rename_file / delete_file / make_dir / list_dir. The first three return 1 on
// success and 0 on failure (like write_file); list_dir raises when the
// directory cannot be opened.

// RAX = status (0 = success) -> pushes the NUMBER 1.0 / 0.0.
static void push_status_as_number(void) {
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int failed = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RAX, 0x3FF0000000000000ull); // 1.0
    int done = emit_jmp_rel32(code);
    emit_patch_jump(code, failed);
    emit_mov_reg_imm64(code, REG_RAX, 0);                     // 0.0
    emit_patch_jump(code, done);
    emit_mov_reg_imm64(code, REG_RBX, TAG_BOOL);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

// Pops a string and leaves its null-terminated copy's address in RAX.
static void pop_path_nullterm(void) {
    emit_pop_reg(code, REG_RSI);
    emit_pop_reg(code, REG_RBX); // tag (ignored)
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8);
    filepath_emit_nullterm(code);
}

void codegen_builtin_rename_file(void) {
    codegen_expression(); // from
    expect(TOKEN_COMMA, "expected ',' after first path");
    codegen_expression(); // to
    expect(TOKEN_RPAREN, "expected ')' after arguments");
    int32_t base = higher_order_scratch_offset();
    pop_path_nullterm();
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RAX); // to
    pop_path_nullterm();
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 24);
    platform_emit_rename(code);
    push_status_as_number();
}

void codegen_builtin_delete_file(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    pop_path_nullterm();
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    platform_emit_delete(code);
    push_status_as_number();
}

void codegen_builtin_make_dir(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    pop_path_nullterm();
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    platform_emit_mkdir(code);
    push_status_as_number();
}

// list_dir(path): array of entry names (no "." / ".."), in directory order.
// Linux walks getdents64 records; Windows walks FindFirstFileA / FindNextFileA.
//
// Scratch: [24]=fd/find handle [32]=dirent / WIN32_FIND_DATA buffer
//          [40]=bytes read [48]=offset [56]=result array object
//          [64]=pattern buffer [72]=path length (Windows)

// An empty result array in [base + 56].
static void emit_new_result_array(int32_t base) {
    emit_mov_reg_imm64(code, REG_RDI, 16);
    heap_emit_alloc(code);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // (temp) data block
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code);
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 32);
    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RSI);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RAX);
}

// RSI = a NUL-terminated entry name. Appends a copy to the result array unless
// it is "." or "..". Clobbers every register.
static void emit_add_entry(int32_t base) {
    emit_load_byte_reg(code, REG_RAX, REG_RSI);
    emit_cmp_reg_imm32(code, REG_RAX, '.');
    int has_name = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_load_byte_reg(code, REG_RAX, REG_RSI);
    emit_sub_reg_imm8(code, REG_RSI, 1);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int skip_dot = emit_jcc_rel32(code, COND_E);
    emit_cmp_reg_imm32(code, REG_RAX, '.');
    int has_name2 = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RSI, 2);
    emit_load_byte_reg(code, REG_RAX, REG_RSI);
    emit_sub_reg_imm8(code, REG_RSI, 2);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int skip_dotdot = emit_jcc_rel32(code, COND_E);
    emit_patch_jump(code, has_name);
    emit_patch_jump(code, has_name2);

    // RDX = strlen(name)
    emit_mov_reg_imm64(code, REG_RDX, 0);
    int len_loop = code->count;
    emit_mov_reg_reg(code, REG_RCX, REG_RSI);
    emit_add_reg_reg(code, REG_RCX, REG_RDX);
    emit_load_byte_reg(code, REG_RAX, REG_RCX);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int len_done = emit_jcc_rel32(code, COND_E);
    emit_add_reg_imm8(code, REG_RDX, 1);
    emit_jmp_back(code, len_loop);
    emit_patch_jump(code, len_done);

    // new string = copy of the name
    emit_mov_reg_reg(code, REG_RBX, REG_RSI); // src (survives string_alloc, as in upper())
    string_alloc_emit_prefixed(code);         // RAX = block, RDX = length
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_add_reg_imm8(code, REG_RDI, 8);
    emit_push_reg(code, REG_RAX);
    bytes_emit_copy(code);                    // RDI=dest, RBX=src, RDX=count
    emit_pop_reg(code, REG_RAX);
    emit_add_reg_imm8(code, REG_RAX, 8);      // string payload

    // push(result, name)
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_mov_reg_reg(code, REG_RCX, REG_RAX);
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 56);
    emit_mov_reg_imm64(code, REG_RDX, TAG_ARRAY);
    emit_push_reg(code, REG_RDX);
    emit_push_reg(code, REG_RSI);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RCX);
    codegen_builtin_push();
    emit_pop_reg(code, REG_RAX); // discard push()'s result
    emit_pop_reg(code, REG_RAX);

    emit_patch_jump(code, skip_dot);
    emit_patch_jump(code, skip_dotdot);
}

static void emit_push_result_array(int32_t base) {
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 56);
    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

#define WIN32_FIND_DATA_SIZE 320
#define WIN32_FIND_DATA_NAME_OFFSET 44

// Windows: pattern = path + "\\*", then FindFirstFileA / FindNextFileA / FindClose.
static void emit_list_dir_windows(int32_t base) {
    emit_pop_reg(code, REG_RSI); // path payload
    emit_pop_reg(code, REG_RBX);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8);
    emit_store_mem_disp32(code, REG_RBP, base + 72, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 80, REG_RSI);
    emit_mov_reg_reg(code, REG_RDI, REG_RDX);
    emit_add_reg_imm8(code, REG_RDI, 3); // "\\", "*", NUL
    heap_emit_alloc(code);
    emit_store_mem_disp32(code, REG_RBP, base + 64, REG_RAX);
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 80);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 72);
    bytes_emit_copy(code);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 64);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 72);
    emit_add_reg_reg(code, REG_RAX, REG_RDX);
    emit_store_byte_imm(code, REG_RAX, '\\');
    emit_add_reg_imm8(code, REG_RAX, 1);
    emit_store_byte_imm(code, REG_RAX, '*');
    emit_add_reg_imm8(code, REG_RAX, 1);
    emit_store_byte_imm(code, REG_RAX, 0);

    emit_new_result_array(base);
    emit_mov_reg_imm64(code, REG_RDI, WIN32_FIND_DATA_SIZE);
    heap_emit_alloc(code);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 64);
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    win_call_begin(code, 0);
    win_call_arg_reg(code, 0, REG_RSI);
    win_call_arg_reg(code, 1, REG_RDI);
    win_call_import(code, PE_IMPORT_FIND_FIRST_FILE_A);
    win_call_end(code);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RAX);
    emit_cmp_reg_imm32(code, REG_RAX, -1); // INVALID_HANDLE_VALUE
    int opened = emit_jcc_rel32(code, COND_NE);
    errors_emit_die(code, "runtime error: cannot open directory");
    emit_patch_jump(code, opened);

    int next_entry = code->count;
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 32);
    emit_add_reg_imm8(code, REG_RSI, WIN32_FIND_DATA_NAME_OFFSET);
    emit_add_entry(base);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 24);
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 32);
    win_call_begin(code, 0);
    win_call_arg_reg(code, 0, REG_RSI);
    win_call_arg_reg(code, 1, REG_RDI);
    win_call_import(code, PE_IMPORT_FIND_NEXT_FILE_A);
    win_call_end(code);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    emit_jcc_back(code, COND_NE, next_entry);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 24);
    win_call_begin(code, 0);
    win_call_arg_reg(code, 0, REG_RSI);
    win_call_import(code, PE_IMPORT_FIND_CLOSE);
    win_call_end(code);
    emit_push_result_array(base);
}

void codegen_builtin_list_dir(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    int32_t base = higher_order_scratch_offset();
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        emit_list_dir_windows(base);
        return;
    }
    pop_path_nullterm();

    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_mov_reg_imm64(code, REG_RSI, 0x10000); // O_RDONLY | O_DIRECTORY
    emit_mov_reg_imm64(code, REG_RDX, 0);
    emit_mov_reg_imm64(code, REG_RAX, 2);       // syscall: open
    emit_syscall(code);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int opened = emit_jcc_rel32(code, COND_GE);
    errors_emit_die(code, "runtime error: cannot open directory");
    emit_patch_jump(code, opened);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RAX);

    emit_new_result_array(base);

    emit_mov_reg_imm64(code, REG_RDI, 4096);
    heap_emit_alloc(code);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // dirent buffer

    int fill = code->count;
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 24);
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 32);
    emit_mov_reg_imm64(code, REG_RDX, 4096);
    emit_mov_reg_imm64(code, REG_RAX, 217); // syscall: getdents64
    emit_syscall(code);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int eof = emit_jcc_rel32(code, COND_LE);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RAX);
    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RCX);

    int next_entry = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 48);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 40);
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int buffer_done = emit_jcc_rel32(code, COND_GE);

    // RSI = entry; advance the stored offset by d_reclen (u16 at +16) up front
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 32);
    emit_add_reg_reg(code, REG_RSI, REG_RCX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 16);
    emit_mov_reg_imm64(code, REG_RDX, 0xFFFF);
    emit_and_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RCX, REG_RAX);
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RCX);
    emit_add_reg_imm8(code, REG_RSI, 19); // d_name

    emit_add_entry(base);
    emit_jmp_back(code, next_entry);

    emit_patch_jump(code, buffer_done);
    emit_jmp_back(code, fill);

    emit_patch_jump(code, eof);
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 24);
    platform_emit_close(code);
    emit_push_result_array(base);
}
