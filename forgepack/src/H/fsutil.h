#ifndef FORGEPACK_FSUTIL_H
#define FORGEPACK_FSUTIL_H

int fsutil_dir_exists(const char *path);

// mkdir -p equivalent -- creates every missing component of `path`.
// Returns 1 on success (including "already exists"), 0 on failure.
int fsutil_mkdir_p(const char *path);

// Recursively copies `src_dir` into `dest_dir` (dest_dir is created).
// Used for "path:" dependencies -- deliberately a copy, not a symlink,
// so an installed @ash-modules/ entry never changes out from under a
// build just because the original source directory did.
int fsutil_copy_dir(const char *src_dir, const char *dest_dir);

#endif
