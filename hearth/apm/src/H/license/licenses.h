#ifndef APM_LICENSES_H
#define APM_LICENSES_H

#include <stddef.h>

// The licenses `apm license` can write, compiled in as C string data (no network, no files to ship).
// Texts are verbatim from the SPDX license list; MIT, BSD-2/3-Clause, ISC and OFL-1.1 contain the
// placeholders {{year}} and {{author}}, which license_render() fills in.
typedef struct {
    const char *id;    // canonical id written to ash.pkg, e.g. "GPL-3.0"
    const char *alias; // also accepted on the command line (SPDX "-only" form, "BSD-3"), or NULL
    const char *name;
    const char *text;
} License;

extern const License licenses_permissive[];
extern const License licenses_copyleft[];

// Case-insensitive lookup by id or alias; NULL when unknown.
const License *license_find(const char *id);

// Calls fn(license) for every available license, permissive ones first.
void license_each(void (*fn)(const License *));

// 1 when the text has an {{author}} placeholder.
int license_needs_author(const License *l);

// Returns the license text with {{year}} and {{author}} substituted (malloc'd, caller frees).
char *license_render(const License *l, const char *year, const char *author);

#endif
