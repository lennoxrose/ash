#ifndef APM_LOCK_H
#define APM_LOCK_H

#include "H/manifest.h"

#define LOCK_FILE "ash.lock"
#define LOCK_MAX_ENTRIES 256

// One package of the lockfile: what ash.pkg (or another package) asked for and the exact commit it resolved to.
typedef struct {
    char name[MANIFEST_NAME_LEN];
    char spec[MANIFEST_VALUE_LEN];    // "github:user/repo[@ref]", as written in ash.pkg / the requiring package
    char sha[48];                     // the full commit id (40 hex digits)
    char via[MANIFEST_NAME_LEN];      // the package that required it; empty for a dependency of the project itself
} LockEntry;

// ash.lock records every package under @ash-modules, direct and transitive, in the order they were resolved:
//
//   # ash.lock -- written by apm, commit it ...
//   lock: 1
//
//   [packages]
//   http: github:acme/http@v1.2.0 3f1c...e9 
//   json: github:acme/json 77ab...01 via http
//
// `apm install` checks out exactly these commits (a tag or branch that has moved since does not matter), `apm update`
// is what moves them. Dependencies of the project (also [dev-dependencies]) have no `via`.
typedef struct {
    LockEntry items[LOCK_MAX_ENTRIES];
    int count;
} Lock;

// Reads ash.lock from `dir`. Returns 0 when there is none; a lockfile that cannot be understood is reported on stderr
// and counts as none (the next install writes a fresh one).
int lock_load(const char *dir, Lock *out);

// The text of the lockfile for `lock` (malloc'd), and whether it equals the file in `dir` (so an install that changes
// nothing does not touch the file).
char *lock_render(const Lock *lock);
int lock_matches_file(const char *dir, const Lock *lock);

// Writes ash.lock. An empty lock removes the file. Returns 1 on success.
int lock_save(const char *dir, const Lock *lock);

LockEntry *lock_find(Lock *lock, const char *name);

// Appends an entry (replacing one of the same name). Returns 0 when the table is full.
int lock_set(Lock *lock, const char *name, const char *spec, const char *sha, const char *via);

// Removes `name` (and keeps the order of the others).
void lock_remove(Lock *lock, const char *name);

#endif
