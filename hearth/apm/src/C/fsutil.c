#define _POSIX_C_SOURCE 200809L // fork, execvp, lstat, setenv under -std=c11
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
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
        // installed/cloned locally when you ran `apm add`.
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

// Runs a program with an argument vector, without a shell: nothing in argv can be taken for shell syntax.
int fsutil_run(char *const argv[]) {
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        setenv("GIT_TERMINAL_PROMPT", "0", 1); // a git that cannot find a repository must fail, not ask for a GitHub login
        execvp(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int fsutil_rm_rf(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 1;
    if (!S_ISDIR(st.st_mode)) return unlink(path) == 0;
    DIR *d = opendir(path);
    if (!d) return 0;
    int ok = 1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char sub[1100];
        snprintf(sub, sizeof(sub), "%s/%s", path, e->d_name);
        if (!fsutil_rm_rf(sub)) ok = 0;
    }
    closedir(d);
    return rmdir(path) == 0 && ok;
}

int fsutil_safe_name(const char *name) {
    size_t n = strlen(name);
    if (n == 0 || n > 100 || name[0] == '.') return 0;
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return 0;
    }
    return 1;
}
