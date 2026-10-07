#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "diagnostics/H/diagnostics.h"

static const char *g_source = NULL;
static const char *g_filename = "source";

#define COL_RED    "\033[38;5;203m"
#define COL_CYAN   "\033[38;5;117m"
#define COL_GRAY   "\033[38;5;242m"
#define COL_GREEN  "\033[38;5;150m"
#define COL_RESET  "\033[0m"
#define COL_BOLD   "\033[1m"

void diagnostics_set_source(const char *source, const char *filename) {
    g_source = source;
    g_filename = filename;
}

DiagnosticsSource diagnostics_save_source(void) {
    DiagnosticsSource saved;
    saved.source = g_source;
    saved.filename = g_filename;
    return saved;
}

void diagnostics_restore_source(DiagnosticsSource saved) {
    g_source = saved.source;
    g_filename = saved.filename;
}

static int use_color(void) {
    return isatty(fileno(stderr));
}

// Computes the TRUE line/column by scanning the original source buffer from
// the start, rather than trusting any resettable per-stage line counter.
static void compute_line_col(const char *pos, int *line, int *col) {
    int l = 1, c = 1;
    const char *p = g_source;
    while (p < pos && *p) {
        if (*p == '\n') { l++; c = 1; } else { c++; }
        p++;
    }
    *line = l;
    *col = c;
}

static void get_source_line(const char *pos, const char **line_start, int *line_len) {
    const char *start = pos;
    while (start > g_source && start[-1] != '\n') start--;
    const char *end = pos;
    while (*end && *end != '\n') end++;
    *line_start = start;
    *line_len = (int)(end - start);
}

static int has_prefix(const char *message, const char *prefix) {
    return strncmp(message, prefix, strlen(prefix)) == 0;
}

static const char *lookup_hint(const char *message) {
    if (has_prefix(message, "undefined variable")) return "did you forget to declare it with `let`?";
    if (has_prefix(message, "undefined function")) return "check the function name is spelled correctly";
    if (has_prefix(message, "index out of bounds")) return "check the array's length before indexing";
    if (has_prefix(message, "key not found")) return "use `has(map, key)` to check before accessing";
    if (has_prefix(message, "could not open file")) return "check the file path is correct";
    if (has_prefix(message, "type error")) return "check the value's type before using it this way";
    if (has_prefix(message, "could not resolve")) return "check the import path is correct and the file exists";
    if (has_prefix(message, "circular import")) return "break the cycle by removing one of the imports in the chain shown above";
    if (has_prefix(message, "namespace")) return "give one of the colliding files/libraries a different name, or move one out of the way";
    if (strstr(message, "expected") != NULL) return "check for a missing token nearby, like `;`, `)`, or `}`";
    return NULL;
}

void diagnostics_report(const char *label, const char *message, const char *pos, int len) {
    int color = use_color();
    if (len <= 0) len = 1;

    if (!g_source || !pos) {
        fprintf(stderr, "%s: %s\n", label, message);
        return;
    }

    int line, col;
    compute_line_col(pos, &line, &col);
    const char *line_start; int line_len;
    get_source_line(pos, &line_start, &line_len);

    char linenum_buf[16];
    snprintf(linenum_buf, sizeof(linenum_buf), "%d", line);
    int gutter_width = (int)strlen(linenum_buf);

    if (color) {
        fprintf(stderr, COL_BOLD COL_RED "%s" COL_RESET COL_BOLD ": %s" COL_RESET "\n", label, message);
        fprintf(stderr, COL_CYAN "  --> %s:%d:%d" COL_RESET "\n", g_filename, line, col);
        fprintf(stderr, COL_GRAY "%*s |" COL_RESET "\n", gutter_width, "");
        fprintf(stderr, COL_GRAY "%s |" COL_RESET " %.*s\n", linenum_buf, line_len, line_start);
        fprintf(stderr, COL_GRAY "%*s |" COL_RESET " ", gutter_width, "");
        for (int i = 1; i < col; i++) fputc(' ', stderr);
        fprintf(stderr, COL_RED);
        for (int i = 0; i < len; i++) fputc('^', stderr);
        fprintf(stderr, COL_RESET "\n");
        const char *hint = lookup_hint(message);
        if (hint) {
            fprintf(stderr, COL_GRAY "%*s |" COL_RESET "\n", gutter_width, "");
            fprintf(stderr, "  = " COL_GREEN COL_BOLD "help" COL_RESET ": %s\n", hint);
        }
    } else {
        fprintf(stderr, "%s: %s\n", label, message);
        fprintf(stderr, "  --> %s:%d:%d\n", g_filename, line, col);
        fprintf(stderr, "%*s |\n", gutter_width, "");
        fprintf(stderr, "%s | %.*s\n", linenum_buf, line_len, line_start);
        fprintf(stderr, "%*s | ", gutter_width, "");
        for (int i = 1; i < col; i++) fputc(' ', stderr);
        for (int i = 0; i < len; i++) fputc('^', stderr);
        fputc('\n', stderr);
        const char *hint = lookup_hint(message);
        if (hint) {
            fprintf(stderr, "%*s |\n", gutter_width, "");
            fprintf(stderr, "  = help: %s\n", hint);
        }
    }
}
