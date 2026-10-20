#ifndef APM_MANIFEST_H
#define APM_MANIFEST_H

#include <stddef.h>

#define MANIFEST_MAX_ENTRIES 64
#define MANIFEST_NAME_LEN 128
#define MANIFEST_VALUE_LEN 256
#define MANIFEST_FILE "ash.pkg"

// One "name: value" line of a section. In [dependencies] / [dev-dependencies] the value is a source spec:
// "owner/repo" or "owner/repo@version" (a package linked on the APMH hub, resolved to an exact commit by the hub) or
// "github:user/repo[@ref]" (ref = tag or branch, no hub involved). Plain version ranges and "path:" aren't supported.
typedef struct {
    char name[MANIFEST_NAME_LEN];
    char value[MANIFEST_VALUE_LEN];
} Entry;
typedef Entry Dependency;

typedef struct {
    Entry items[MANIFEST_MAX_ENTRIES];
    int count;
} EntryList;

// ash.pkg is a flat "key: value" header followed by "[section]" blocks of "name: value" lines
// (blank lines and '#' comments are ignored):
//
//   name: my-app                     name, version      identity
//   version: 1.2.0
//   description: What it does        description, author, license (SPDX id), homepage,
//   author: Ada <ada@example.com>    repository, bugs   metadata, as in package.json
//   license: MIT
//   main: demo.Main                  main               entry class (qualified) for `apm run start`-style tooling
//   keywords: ash, cli               keywords           comma-separated
//   memory-safety: on                memory-safety      on (default) | off: ownership/borrow checks and freeing, read by ashvm and kiln
//
//   [dependencies]                   packages fetched into ./@ash-modules/<name>
//   ash-stdlib: github:user/ash-stdlib@v0.1.0
//   [dev-dependencies]               same, only needed to build and test
//   [libraries]                      external native libraries the package links: name: how to find it
//   m: system                        (recorded and listed; not yet consumed by ashvm or kiln)
//   [scripts]                        named shell commands, run with `apm run <name>`
//   build: ashvm check src/main/ash/demo/Main.ash
typedef struct {
    char name[MANIFEST_NAME_LEN];
    char version[32];
    char description[MANIFEST_VALUE_LEN];
    char author[MANIFEST_NAME_LEN];
    char license[64];
    char homepage[MANIFEST_VALUE_LEN];
    char repository[MANIFEST_VALUE_LEN];
    char bugs[MANIFEST_VALUE_LEN];
    char main[MANIFEST_NAME_LEN];
    char keywords[MANIFEST_VALUE_LEN];
    char memory_safety[8];
    EntryList deps;
    EntryList dev_deps;
    EntryList libs;
    EntryList scripts;
} Manifest;

void manifest_init_default(Manifest *m, const char *project_name);

// Reads ash.pkg from `dir` (a directory path, not the file itself).
// Returns 1 on success, 0 if the file doesn't exist.
int manifest_load(const char *dir, Manifest *out);

// Writes ash.pkg into `dir`, overwriting any existing one.
int manifest_save(const char *dir, const Manifest *m);

// Adds a new entry or overwrites an existing one with the same name.
// Returns 0 if the table is full.
int entry_set(EntryList *list, const char *name, const char *value);

// Returns NULL if no entry with that name exists.
Entry *entry_find(EntryList *list, const char *name);

#endif
