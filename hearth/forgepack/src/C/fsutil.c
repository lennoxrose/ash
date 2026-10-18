#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "H/fsutil.h"

int fsutil_dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int fsutil_mkdir_p(const char *path) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", path);
    size_t len = strlen(buf);
    for (size_t i = 1; i <= len; i++) {
        if (buf[i] == '/' || i == len) {
            char save = buf[i];
            buf[i] = '\0';
            if (buf[0] != '\0' && mkdir(buf, 0755) != 0) {
                // EEXIST is fine -- anything else means a real failure.
                struct stat st;
                if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
            }
            buf[i] = save;
        }
    }
    return 1;
}

static int copy_file(const char *src, const char *dest) {
    FILE *in = fopen(src, "rb");
    if (!in) return 0;
    FILE *out = fopen(dest, "wb");
    if (!out) { fclose(in); return 0; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    return 1;
}

int fsutil_copy_dir(const char *src_dir, const char *dest_dir) {
    if (!fsutil_mkdir_p(dest_dir)) return 0;

    DIR *d = opendir(src_dir);
    if (!d) return 0;

    struct dirent *entry;
    int ok = 1;
    while (ok && (entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        // Never copy a nested @ash-modules/ or .git -- a "path:" dependency
        // installs its OWN source, not whatever it happened to have
        // installed/cloned locally when you ran `forgepack add`.
        if (strcmp(entry->d_name, "@ash-modules") == 0 || strcmp(entry->d_name, ".git") == 0) continue;

        char src_path[1024], dest_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name);

        if (fsutil_dir_exists(src_path)) {
            ok = fsutil_copy_dir(src_path, dest_path);
        } else {
            ok = copy_file(src_path, dest_path);
        }
    }
    closedir(d);
    return ok;
}
