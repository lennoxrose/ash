#ifndef PYRE_JSON_OUT_H
#define PYRE_JSON_OUT_H
// --json output for the `pyre` tool: one JSON object per line (the dashboard's feed).
#include <stdio.h>

extern int json_mode;  // defined in pyre.c

static inline void jstr(const char *v) {
    putchar('"');
    for (; *v; v++) {
        unsigned char c = (unsigned char)*v;
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}

// Starts an event line: {"event":"<name>" -- append jkey("k") + a value, finish with jend().
static inline void jbegin(const char *event) { printf("{\"event\":"); jstr(event); }
static inline void jend(void) { printf("}\n"); fflush(stdout); }
static inline void jkey(const char *k) { printf(",\"%s\":", k); }

#endif
