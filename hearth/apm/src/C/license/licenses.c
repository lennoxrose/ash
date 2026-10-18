#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "H/license/licenses.h"

const License *license_find(const char *id) {
    const License *tables[] = { licenses_permissive, licenses_copyleft };
    for (int t = 0; t < 2; t++) {
        for (const License *l = tables[t]; l->id; l++) {
            if (strcasecmp(l->id, id) == 0 || (l->alias && strcasecmp(l->alias, id) == 0)) return l;
        }
    }
    return NULL;
}

void license_each(void (*fn)(const License *)) {
    for (const License *l = licenses_permissive; l->id; l++) fn(l);
    for (const License *l = licenses_copyleft; l->id; l++) fn(l);
}

int license_needs_author(const License *l) {
    return strstr(l->text, "{{author}}") != NULL;
}

char *license_render(const License *l, const char *year, const char *author) {
    size_t cap = strlen(l->text) + strlen(year) + strlen(author) + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    char *w = out;
    for (const char *p = l->text; *p;) {
        if (strncmp(p, "{{year}}", 8) == 0) { w += sprintf(w, "%s", year); p += 8; }
        else if (strncmp(p, "{{author}}", 10) == 0) { w += sprintf(w, "%s", author); p += 10; }
        else *w++ = *p++;
    }
    *w = '\0';
    return out;
}
