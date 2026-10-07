#define _XOPEN_SOURCE 700 // exposes realpath() and PATH_MAX under -std=c11
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "compiler/H/import_paths.h"

void import_path_dirname(const char *path, char *out, int out_size) {
    const char *slash = strrchr(path, '/');
    if (!slash) { snprintf(out, (size_t)out_size, "."); return; }
    int len = (int)(slash - path);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, path, (size_t)len);
    out[len] = '\0';
}

void import_derive_namespace(const char *canonical_path, char *out, int out_size) {
    const char *slash = strrchr(canonical_path, '/');
    const char *base = slash ? slash + 1 : canonical_path;
    int len = (int)strlen(base);
    if (len > 4 && strcmp(base + len - 4, ".ash") == 0) len -= 4;
    if (len >= out_size) len = out_size - 1;
    memcpy(out, base, (size_t)len);
    out[len] = '\0';
}

static int is_relative_prefix(const char *raw, int len) {
    if (len >= 2 && raw[0] == '.' && raw[1] == '/') return 1;
    if (len >= 3 && raw[0] == '.' && raw[1] == '.' && raw[2] == '/') return 1;
    return 0;
}

int import_resolve_path(const char *raw_path, int raw_len,
                         const char *importing_file_dir,
                         const char *project_root_dir,
                         char *canonical_out,
                         char *message_out, int message_out_size) {
    char candidate[PATH_MAX];

    if (is_relative_prefix(raw_path, raw_len)) {
        if (raw_len < 4 || strncmp(raw_path + raw_len - 4, ".ash", 4) != 0) {
            snprintf(message_out, (size_t)message_out_size, "relative import path must end in '.ash'");
            return 0;
        }
        snprintf(candidate, sizeof(candidate), "%s/%.*s", importing_file_dir, raw_len, raw_path);
    } else {
        for (int i = 0; i < raw_len; i++) {
            if (raw_path[i] == '.' || raw_path[i] == '/') {
                snprintf(message_out, (size_t)message_out_size,
                         "library import name must not contain '.' or '/' (did you mean './%.*s'?)", raw_len, raw_path);
                return 0;
            }
        }
        snprintf(candidate, sizeof(candidate), "%s/ash.libs/%.*s/%.*s.ash",
                  project_root_dir, raw_len, raw_path, raw_len, raw_path);
    }

    if (realpath(candidate, canonical_out) == NULL) {
        snprintf(message_out, (size_t)message_out_size, "could not resolve '%.*s'", raw_len, raw_path);
        return 0;
    }
    return 1;
}
