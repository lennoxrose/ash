#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler/H/module_state.h"
#include "compiler/H/internal.h"
#include "vm/H/state.h"
#include "diagnostics/H/diagnostics.h"

static char names[MAX_VM_GLOBALS][96];
static int count = 0;

int module_state_resolve_qualified(const char *name, int len) {
    for (int i = 0; i < count; i++) {
        if ((int)strlen(names[i]) == len && strncmp(names[i], name, (size_t)len) == 0) return i;
    }
    return -1;
}

int module_state_resolve(const char *name, int len) {
    const char *ns; int ns_len;
    get_import_namespace(&ns, &ns_len);
    if (ns_len == 0) return -1;
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "%.*s.%.*s", ns_len, ns, len, name);
    if (n < 0 || n >= (int)sizeof(buf)) return -1;
    return module_state_resolve_qualified(buf, n);
}

int module_state_declare(const char *name, int len) {
    const char *ns; int ns_len;
    get_import_namespace(&ns, &ns_len);
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "%.*s.%.*s", ns_len, ns, len, name);
    if (n < 0 || n >= (int)sizeof(names[0])) {
        diagnostics_report("error", "module-level name too long (including namespace prefix)", current.start, current.length);
        exit(1);
    }
    if (module_state_resolve_qualified(buf, n) != -1) {
        diagnostics_report("error", "module-level name already declared", current.start, current.length);
        exit(1);
    }
    if (count >= MAX_VM_GLOBALS) {
        diagnostics_report("error", "too many module-level variables", current.start, current.length);
        exit(1);
    }
    memcpy(names[count], buf, (size_t)n);
    names[count][n] = '\0';
    return count++;
}
