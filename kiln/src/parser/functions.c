#include <string.h>
#include <stdio.h>
#include "parser/functions.h"
#include "parser/parser.h"

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

KilnFunction *declare_function_in_context(const char *name, int len) {
    if (g_import_ns_len == 0) return declare_function(name, len);
    char buf[128];
    int combined_len = build_namespaced_name(buf, sizeof(buf), name, len);
    if (combined_len < 0) parse_error("function name too long (including namespace prefix)");
    return declare_function(buf, combined_len);
}

KilnFunction *resolve_function_in_context(const char *name, int len) {
    if (g_import_ns_len == 0) return resolve_function(name, len);
    char buf[128];
    int combined_len = build_namespaced_name(buf, sizeof(buf), name, len);
    if (combined_len < 0) return NULL;
    return resolve_function(buf, combined_len);
}
