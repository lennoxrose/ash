#include <stdlib.h>
#include <string.h>
#include "compiler/state.h"
#include "diagnostics/diagnostics.h"

VMFunction vm_functions[MAX_VM_FUNCS];
int vm_function_count = 0;

Token current;
Token previous;
Chunk *chunk;
char local_names[MAX_VM_LOCALS][64];
int local_count;
int lambda_counter = 0;

void advance_token(void) { previous = current; current = lexer_next_token(); }

void expect(TokenType type, const char *message) {
    if (current.type == type) { advance_token(); return; }
    diagnostics_report("error", message, current.start, current.length);
    exit(1);
}

int resolve_local(const char *name, int len) {
    for (int i = 0; i < local_count; i++) {
        if ((int)strlen(local_names[i]) == len && strncmp(local_names[i], name, len) == 0) return i;
    }
    return -1;
}

int declare_local(const char *name, int len) {
    if (local_count >= MAX_VM_LOCALS) { diagnostics_report("error", "too many locals", current.start, current.length); exit(1); }
    memcpy(local_names[local_count], name, len);
    local_names[local_count][len] = '\0';
    return local_count++;
}

int find_function(const char *name, int len) {
    for (int i = 0; i < vm_function_count; i++) {
        if ((int)strlen(vm_functions[i].name) == len && strncmp(vm_functions[i].name, name, len) == 0) return i;
    }
    return -1;
}

static const char *g_import_ns = NULL;
static int g_import_ns_len = 0;

void set_import_namespace(const char *ns, int len) {
    g_import_ns = ns;
    g_import_ns_len = len;
}

void get_import_namespace(const char **out_ns, int *out_len) {
    *out_ns = g_import_ns;
    *out_len = g_import_ns_len;
}

static int build_namespaced_name(char *buf, int buf_size, const char *name, int len) {
    if (g_import_ns_len + 1 + len >= buf_size) return -1;
    memcpy(buf, g_import_ns, (size_t)g_import_ns_len);
    buf[g_import_ns_len] = '.';
    memcpy(buf + g_import_ns_len + 1, name, (size_t)len);
    return g_import_ns_len + 1 + len;
}

int declare_function_in_context(const char *name, int len) {
    char buf[128];
    const char *final_name = name;
    int final_len = len;
    if (g_import_ns_len > 0) {
        int combined_len = build_namespaced_name(buf, sizeof(buf), name, len);
        if (combined_len < 0) { diagnostics_report("error", "function name too long (including namespace prefix)", current.start, current.length); exit(1); }
        final_name = buf;
        final_len = combined_len;
    }
    if (vm_function_count >= MAX_VM_FUNCS) { diagnostics_report("error", "too many functions", current.start, current.length); exit(1); }
    if (final_len >= (int)sizeof(vm_functions[0].name)) { diagnostics_report("error", "function name too long", current.start, current.length); exit(1); }
    int fn_idx = vm_function_count++;
    memcpy(vm_functions[fn_idx].name, final_name, (size_t)final_len);
    vm_functions[fn_idx].name[final_len] = '\0';
    return fn_idx;
}

int find_function_in_context(const char *name, int len) {
    if (g_import_ns_len == 0) return find_function(name, len);
    char buf[128];
    int combined_len = build_namespaced_name(buf, sizeof(buf), name, len);
    if (combined_len < 0) return -1;
    return find_function(buf, combined_len);
}

void emit(uint8_t byte) { chunk_write(chunk, byte); }
void emit_op(uint8_t op) { chunk_write_pos(chunk, op, previous.start, previous.length); }
void emit2(uint8_t a, uint8_t b) { emit_op(a); emit(b); }

void emit_constant(VMValue v) {
    int idx = chunk_add_constant(chunk, v);
    emit2(OP_CONST, (uint8_t)idx);
}

int emit_jump(uint8_t op) { emit_op(op); emit(0xff); emit(0xff); return chunk->count - 2; }

void patch_jump(int offset) {
    int jump = chunk->count - offset - 2;
    chunk->code[offset] = (jump >> 8) & 0xff;
    chunk->code[offset + 1] = jump & 0xff;
}

void emit_loop(int loop_start) {
    emit_op(OP_LOOP);
    int offset = chunk->count - loop_start + 2;
    emit((offset >> 8) & 0xff);
    emit(offset & 0xff);
}
