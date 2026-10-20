#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "jexplain/H/jexplain.h"

static int same_code(const char *a, const char *b) {
    for (; *a && *b; a++, b++) if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
    return !*a && !*b;
}

int jexplain_print(const char *code) {
    if (!code) {
        for (const JExplain *e = jexplain_table; e->code; e++) printf("%s  %s\n", e->code, e->title);
        return 0;
    }
    for (const JExplain *e = jexplain_table; e->code; e++) {
        if (!same_code(e->code, code)) continue;
        printf("%s: %s\n\n%s", e->code, e->title, e->text);
        return 0;
    }
    fprintf(stderr, "no such error code: %s (run with no code to list them)\n", code);
    return 1;
}
