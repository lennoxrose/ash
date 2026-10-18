#ifndef JCORE_H
#define JCORE_H

/* Types the compiler itself depends on (Object, String, the boxes, exceptions, Iterable/Iterator,
 * the functional interfaces). They are compiled into the front end as C string data and parsed
 * lazily, only when a program reaches them; nothing is read from disk. Everything else in the
 * standard library (ash.util, ash.io, ash.json, ...) is the optional ash-stdlib package found
 * through the source roots (see jt_init_default_roots).
 *
 * Ownership: returned strings are static and read-only. */

typedef struct {
    const char *name;   /* source path without extension, e.g. "ash/lang/Object" */
    const char *source;
} JCoreEntry;

extern const JCoreEntry jcore_lang[];
extern const JCoreEntry jcore_function[];

/* Source of the core type in the file `rel` ("ash/lang/Object"), or NULL when it is not a core type. */
const char *jcore_source(const char *rel);

#endif
