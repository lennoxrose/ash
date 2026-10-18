#ifndef FORGEPACK_MANIFEST_H
#define FORGEPACK_MANIFEST_H

#define MANIFEST_MAX_DEPS 64
#define MANIFEST_NAME_LEN 128
#define MANIFEST_VALUE_LEN 256
#define MANIFEST_FILE "ash.pkg"

// A dependency's value is a raw spec string. GitHub-only for now --
// "github:user/repo" or "github:user/repo@ref" (ref = tag or branch).
// No registry yet (see ideas/ash_modules.md), so a plain version/range
// or a local "path:" dependency aren't supported yet either.
typedef struct {
    char name[MANIFEST_NAME_LEN];
    char value[MANIFEST_VALUE_LEN];
} Dependency;

typedef struct {
    char name[MANIFEST_NAME_LEN];
    char version[32];
    Dependency deps[MANIFEST_MAX_DEPS];
    int dep_count;
} Manifest;

void manifest_init_default(Manifest *m, const char *project_name);

// Reads ash.pkg from `dir` (a directory path, not the file itself).
// Returns 1 on success, 0 if the file doesn't exist or is malformed
// (a message is printed to stderr either way).
int manifest_load(const char *dir, Manifest *out);

// Writes ash.pkg into `dir`, overwriting any existing one.
int manifest_save(const char *dir, const Manifest *m);

// Adds a new dependency or overwrites an existing one with the same name.
// Returns 0 if the table is full.
int manifest_set_dependency(Manifest *m, const char *name, const char *value);

// Returns NULL if no dependency with that name exists.
Dependency *manifest_find_dependency(Manifest *m, const char *name);

#endif
