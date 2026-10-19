#ifndef APM_FSUTIL_H
#define APM_FSUTIL_H

int fsutil_dir_exists(const char *path);

// mkdir -p equivalent -- creates every missing component of `path`.
// Returns 1 on success (including "already exists"), 0 on failure.
int fsutil_mkdir_p(const char *path);

// Recursively copies `src_dir` into `dest_dir` (dest_dir is created).
// Used for "path:" dependencies -- deliberately a copy, not a symlink,
// so an installed @ash-modules/ entry never changes out from under a
// build just because the original source directory did.
int fsutil_copy_dir(const char *src_dir, const char *dest_dir);

// Runs argv[0] with the given arguments and no shell. Returns 1 when it exited with status 0.
int fsutil_run(char *const argv[]);

// rm -rf without a shell (symbolic links are removed, never followed). Returns 1 when `path` is gone.
int fsutil_rm_rf(const char *path);

// A name that is safe as one path component and in a command line: letters, digits, '-', '_', '.', not
// starting with '.', at most 100 characters. Dependency names become directories under @ash-modules.
int fsutil_safe_name(const char *name);

#endif
