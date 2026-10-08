#include <string.h>
#include <stdio.h>
#include "parser/H/declarations/functions.h"
#include "parser/H/core/parser.h"
#include "lexer/H/lexer.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/layout.h"

static KilnFunction functions[MAX_KILN_FUNCS];
static int function_count = 0;

static const char *g_import_ns = NULL;
static int g_import_ns_len = 0;

KilnFunction *resolve_function(const char *name, int len) {
    for (int i = 0; i < function_count; i++) {
        if ((int)strlen(functions[i].name) == len && strncmp(functions[i].name, name, len) == 0) return &functions[i];
    }
    return NULL;
}

KilnFunction *declare_function(const char *name, int len) {
    if (function_count >= MAX_KILN_FUNCS) parse_error("too many functions");
    if (resolve_function(name, len) != NULL) parse_error("function already declared");
    if (len >= (int)sizeof(functions[0].name)) parse_error("function name too long (including any namespace prefix)");
    KilnFunction *fn = &functions[function_count++];
    memcpy(fn->name, name, (size_t)len);
    fn->name[len] = '\0';
    fn->code_offset = 0;
    fn->arity = 0;
    fn->defined = 1;
    return fn;
}

void set_import_namespace(const char *ns, int len) {
    g_import_ns = ns;
    g_import_ns_len = len;
}

void get_import_namespace(const char **out_ns, int *out_len) {
    *out_ns = g_import_ns;
    *out_len = g_import_ns_len;
}

// Shared by declare_function_in_context/resolve_function_in_context --
// writes "ns.name" into buf (bounded by buf_size) and returns its length,
// or -1 if it doesn't fit.
static int build_namespaced_name(char *buf, int buf_size, const char *name, int len) {
    if (g_import_ns_len + 1 + len >= buf_size) return -1;
    memcpy(buf, g_import_ns, (size_t)g_import_ns_len);
    buf[g_import_ns_len] = '.';
    memcpy(buf + g_import_ns_len + 1, name, (size_t)len);
    return g_import_ns_len + 1 + len;
}

// A prescan (prescan_functions below) may already have reserved the name so
// earlier code could call it; the real declaration then fills that entry in.
static KilnFunction *declare_or_define(const char *name, int len) {
    KilnFunction *existing = resolve_function(name, len);
    if (existing != NULL && !existing->defined) {
        existing->defined = 1;
        return existing;
    }
    return declare_function(name, len);
}

KilnFunction *declare_function_in_context(const char *name, int len) {
    if (g_import_ns_len == 0) return declare_or_define(name, len);
    char buf[128];
    int combined_len = build_namespaced_name(buf, sizeof(buf), name, len);
    if (combined_len < 0) parse_error("function name too long (including namespace prefix)");
    return declare_or_define(buf, combined_len);
}

void predeclare_function_in_context(const char *name, int len, int arity) {
    const char *final_name = name;
    int final_len = len;
    char buf[128];
    if (g_import_ns_len > 0) {
        final_len = build_namespaced_name(buf, sizeof(buf), name, len);
        if (final_len < 0) return; // the real declaration reports it
        final_name = buf;
    }
    if (resolve_function(final_name, final_len) != NULL) return;
    if (function_count >= MAX_KILN_FUNCS || final_len >= (int)sizeof(functions[0].name)) return;
    KilnFunction *fn = &functions[function_count++];
    memcpy(fn->name, final_name, (size_t)final_len);
    fn->name[final_len] = '\0';
    fn->code_offset = 0;
    fn->arity = arity;
    fn->defined = 0;
}

void prescan_functions(const char *source) {
    LexerState saved = lexer_save_state();
    lexer_init(source);
    Token t = lexer_next_token();
    while (t.type != TOKEN_EOF) {
        if (t.type == TOKEN_FORGE) {
            Token name = lexer_next_token();
            if (name.type != TOKEN_IDENTIFIER) { t = name; continue; }
            Token paren = lexer_next_token();
            if (paren.type != TOKEN_LPAREN) { t = paren; continue; }
            int arity = 0;
            t = lexer_next_token();
            if (t.type != TOKEN_RPAREN) {
                arity = 1;
                while (t.type != TOKEN_RPAREN && t.type != TOKEN_EOF) {
                    if (t.type == TOKEN_COMMA) arity++;
                    t = lexer_next_token();
                }
            }
            predeclare_function_in_context(name.start, name.length, arity);
        }
        if (t.type != TOKEN_EOF) t = lexer_next_token();
    }
    lexer_restore_state(saved);
}

#define MAX_KILN_FIXUPS 4096
static struct { KilnFunction *fn; int patch_offset; int absolute; } fixups[MAX_KILN_FIXUPS];
static int fixup_count = 0;

static void add_fixup(KilnFunction *fn, int patch_offset, int absolute) {
    if (fixup_count >= MAX_KILN_FIXUPS) parse_error("too many calls to functions defined later");
    fixups[fixup_count].fn = fn;
    fixups[fixup_count].patch_offset = patch_offset;
    fixups[fixup_count].absolute = absolute;
    fixup_count++;
}
void function_add_call_fixup(KilnFunction *fn, int patch_offset) { add_fixup(fn, patch_offset, 0); }
void function_add_abs_fixup(KilnFunction *fn, int patch_offset) { add_fixup(fn, patch_offset, 1); }

void function_resolve_fixups(KilnFunction *fn) {
    int kept = 0;
    for (int i = 0; i < fixup_count; i++) {
        if (fixups[i].fn != fn) { fixups[kept++] = fixups[i]; continue; }
        if (fixups[i].absolute) {
            emit_patch_imm64(code, fixups[i].patch_offset, kiln_code_base() + (uint64_t)fn->code_offset);
        } else {
            int32_t rel = fn->code_offset - (fixups[i].patch_offset + 4);
            memcpy(code->code + fixups[i].patch_offset, &rel, 4);
        }
    }
    fixup_count = kept;
}

void check_all_functions_defined(void) {
    for (int i = 0; i < function_count; i++) {
        if (!functions[i].defined) {
            char msg[128];
            snprintf(msg, sizeof(msg), "undefined function: %s", functions[i].name);
            parse_error(msg);
        }
    }
}

KilnFunction *resolve_function_in_context(const char *name, int len) {
    if (g_import_ns_len == 0) return resolve_function(name, len);
    char buf[128];
    int combined_len = build_namespaced_name(buf, sizeof(buf), name, len);
    if (combined_len < 0) return NULL;
    return resolve_function(buf, combined_len);
}
