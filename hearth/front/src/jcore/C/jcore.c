#include <string.h>

#include "jcore/H/jcore.h"

static const char *find(const JCoreEntry *table, const char *rel) {
    for (; table->name; table++)
        if (strcmp(table->name, rel) == 0) return table->source;
    return NULL;
}

const char *jcore_source(const char *rel) {
    if (strncmp(rel, "ash/lang/function/", 18) == 0) return find(jcore_function, rel);
    const char *s = find(jcore_lang, rel);
    return s;
}
