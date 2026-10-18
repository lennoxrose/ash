#define _POSIX_C_SOURCE 200809L // exposes fileno() under -std=c11
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include "H/ui.h"

#define GREEN "\x1b[32m"
#define RED "\x1b[31m"
#define CYAN "\x1b[36m"
#define DIM "\x1b[2m"
#define BOLD "\x1b[1m"
#define RESET "\x1b[0m"

static int color_enabled(FILE *stream) {
    static int checked = 0, out_tty = -1, err_tty = -1, no_color = -1;
    if (!checked) {
        out_tty = isatty(fileno(stdout));
        err_tty = isatty(fileno(stderr));
        no_color = getenv("NO_COLOR") != NULL;
        checked = 1;
    }
    if (no_color) return 0;
    return stream == stdout ? out_tty : err_tty;
}

static void emit(FILE *stream, const char *color, const char *symbol, const char *fmt, va_list args) {
    int c = color_enabled(stream);
    if (c) fprintf(stream, "%s%s%s ", color, symbol, RESET);
    else fprintf(stream, "%s ", symbol);
    vfprintf(stream, fmt, args);
    fprintf(stream, "\n");
}

void ui_ok(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    emit(stdout, GREEN, "\xe2\x9c\x93", fmt, args); // U+2713 CHECK MARK
    va_end(args);
}

void ui_err(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    emit(stderr, RED, "\xe2\x9c\x97", fmt, args); // U+2717 BALLOT X
    va_end(args);
}

void ui_info(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    emit(stdout, CYAN, "\xe2\x86\x92", fmt, args); // U+2192 RIGHTWARDS ARROW
    va_end(args);
}

void ui_bullet(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    emit(stdout, DIM, "\xe2\x80\xa2", fmt, args); // U+2022 BULLET
    va_end(args);
}

void ui_header(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    int c = color_enabled(stdout);
    if (c) printf("%s", BOLD);
    vprintf(fmt, args);
    if (c) printf("%s", RESET);
    printf("\n");
    va_end(args);
}
