#include <stdlib.h>
#include <string.h>
#include "builtins/H/internal.h"
#include "diagnostics/H/diagnostics.h"
#include "vm/H/state.h"
#include "vm/H/vm.h"

// trace_n() / trace_fn(i) / trace_line(i): the stack captured at the last uncaught raise (vm_capture_trace), as
// the named frames of project code. The lowering writes `//@<line> <Class.method>(<File.ash>` as the first line of a
// project function and `//@<line>` before each statement; a frame is located by scanning the IR text backwards from
// its instruction. Only the error path ever gets here.

typedef struct {
    const char *name;   // "t.Main.get(Main.ash", not terminated
    int name_len;
    int line;
} Site;

static Site sites[FRAMES_MAX];       // one per captured frame; name == NULL for a frame that is not project code
static int site_count;
static unsigned resolved_gen;

// Is `p` the start of a marker line "//@..."? (indentation allowed)
static const char *marker_at(const char *line_start, const char *limit) {
    const char *p = line_start;
    while (p < limit && (*p == ' ' || *p == '\t')) p++;
    if (limit - p >= 3 && p[0] == '/' && p[1] == '/' && p[2] == '@') return p + 3;
    return NULL;
}

static int is_function_header(const char *line_start, const char *limit) {
    return limit - line_start >= 6 && !strncmp(line_start, "forge ", 6);
}

// Finds the line and name for the instruction text at `at` (inside the IR source `text`). Returns 0 for code that is
// not a project function (prelude, library, top level).
static int locate(const char *text, const char *at, Site *out) {
    const char *p = at;
    int line = 0;
    const char *end = at;
    while (*end && *end != '\n') end++;
    // walk back over whole lines, from the one containing `at`
    for (;;) {
        const char *ls = p;
        while (ls > text && ls[-1] != '\n') ls--;
        const char *le = ls;
        while (*le && *le != '\n') le++;
        if (is_function_header(ls, le)) return 0;       // reached the function's first line without finding a name
        const char *m = marker_at(ls, le);
        if (m) {
            int v = 0, digits = 0;
            while (m < le && *m >= '0' && *m <= '9') { v = v * 10 + (*m - '0'); m++; digits++; }
            if (digits) {
                if (!line) line = v;
                if (m < le && *m == ' ') {              // a named marker: the function's first line
                    out->name = m + 1;
                    out->name_len = (int)(le - (m + 1));
                    out->line = line;
                    return 1;
                }
            }
        }
        if (ls == text) return 0;
        p = ls - 1;                                     // the previous line
    }
}

static void resolve(void) {
    if (resolved_gen == trace_gen) return;
    site_count = 0;
    DiagnosticsSource ds = diagnostics_save_source();
    const char *text = ds.source;
    for (int i = 0; text && i < trace_count; i++) {
        Chunk *c = trace_chunk[i];
        Site s = {0};
        long off = trace_ip[i] - c->code;
        if (off > 0 && off <= c->count) {
            long k = off - 1;
            while (k > 0 && !c->pos[k]) k--;            // back to the opcode that was running
            const char *at = c->pos[k];
            if (!at || at < text || !locate(text, at, &s)) s.name = NULL;
        }
        sites[site_count++] = s;
    }
    resolved_gen = trace_gen;
}

VMValue vm_call_builtin_trace(int id, VMValue *args, int argc) {
    resolve();
    if (id == 55) return vm_num(site_count);
    if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("trace_fn(i) / trace_line(i) expect an index\n"); }
    int i = (int)args[0].number;
    if (i < 0 || i >= site_count) { vm_runtime_error("trace index out of range\n"); }
    if (id == 57) return vm_num(sites[i].name ? sites[i].line : 0);
    if (!sites[i].name) return vm_str(calloc(1, 1));
    char *copy = malloc((size_t)sites[i].name_len + 1);
    memcpy(copy, sites[i].name, (size_t)sites[i].name_len);
    copy[sites[i].name_len] = '\0';
    return vm_str(copy);
}
