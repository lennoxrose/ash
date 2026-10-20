#define _XOPEN_SOURCE 700 // poll(), fork(), setenv() under -std=c11
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "H/hub/proc.h"

static void child_exec(char *const argv[], const char *const *env, int in_fd, int out_fd) {
    dup2(in_fd, 0);
    if (out_fd >= 0) dup2(out_fd, 1);
    for (int fd = 3; fd < 64; fd++) close(fd); // inherit nothing else
    for (; env && *env; env++) {
        char key[128];
        const char *eq = strchr(*env, '=');
        if (eq && (size_t)(eq - *env) < sizeof(key)) {
            memcpy(key, *env, (size_t)(eq - *env));
            key[eq - *env] = '\0';
            setenv(key, eq + 1, 1);
        }
    }
    execvp(argv[0], argv);
    _exit(127);
}

int proc_run(char *const argv[], const char *input, size_t input_len, char **out, size_t max_out, const char *const *extra_env) {
    int in_pipe[2], out_pipe[2];
    *out = NULL;
    if (pipe(in_pipe) != 0) return -1;
    if (pipe(out_pipe) != 0) { close(in_pipe[0]); close(in_pipe[1]); return -1; }
    void (*old_sigpipe)(int) = signal(SIGPIPE, SIG_IGN); // a child that exits early must not kill apm
    pid_t pid = fork();
    if (pid < 0) {
        close(in_pipe[0]); close(in_pipe[1]); close(out_pipe[0]); close(out_pipe[1]);
        signal(SIGPIPE, old_sigpipe);
        return -1;
    }
    if (pid == 0) {
        close(in_pipe[1]); close(out_pipe[0]);
        child_exec(argv, extra_env, in_pipe[0], out_pipe[1]);
    }
    close(in_pipe[0]); close(out_pipe[1]);

    size_t written = 0, got = 0, cap = 4096;
    char *buf = malloc(cap);
    int fail = buf == NULL, in_open = 1;
    if (!input || input_len == 0) { close(in_pipe[1]); in_open = 0; }
    while (!fail) { // write the input and read the output together, so neither side can block the other
        struct pollfd fds[2] = { { out_pipe[0], POLLIN, 0 }, { in_open ? in_pipe[1] : -1, POLLOUT, 0 } };
        if (poll(fds, 2, -1) < 0 && errno != EINTR) { fail = 1; break; }
        if (in_open && (fds[1].revents & (POLLOUT | POLLERR | POLLHUP))) {
            ssize_t w = write(in_pipe[1], input + written, input_len - written);
            if (w > 0) written += (size_t)w;
            if (w < 0 || written >= input_len) { close(in_pipe[1]); in_open = 0; }
        }
        if (fds[0].revents & (POLLIN | POLLHUP)) {
            if (got + 1024 >= cap) {
                if (cap >= max_out + 4096) { fail = 1; break; }
                char *bigger = realloc(buf, cap * 2);
                if (!bigger) { fail = 1; break; }
                buf = bigger; cap *= 2;
            }
            ssize_t r = read(out_pipe[0], buf + got, cap - got - 1);
            if (r < 0 && errno != EINTR) { fail = 1; break; }
            if (r == 0) break;
            if (r > 0) got += (size_t)r;
            if (got > max_out) { fail = 1; break; }
        }
    }
    if (in_open) close(in_pipe[1]);
    close(out_pipe[0]);
    int status = 0;
    if (fail) kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    signal(SIGPIPE, old_sigpipe);
    if (fail) { free(buf); return -1; }
    buf[got] = '\0';
    *out = buf;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int proc_quiet(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        int null = open("/dev/null", O_RDWR);
        if (null < 0) _exit(127);
        dup2(null, 0); dup2(null, 1); dup2(null, 2);
        for (int fd = 3; fd < 64; fd++) close(fd);
        execvp(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
