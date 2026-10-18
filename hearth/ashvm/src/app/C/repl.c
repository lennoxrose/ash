#define _DEFAULT_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "app/H/repl.h"
#include "jdriver/H/jdriver.h"
#include "jrepl/H/jrepl.h"

// Tracks { } nesting across lines, ignoring braces inside string and char literals.
static void count_braces(const char *line, int *depth, char *quote) {
    for (const char *p = line; *p; p++) {
        if (*quote) {
            if (*p == '\\' && p[1] != '\0') { p++; continue; }
            if (*p == *quote) *quote = 0;
            continue;
        }
        if (*p == '"' || *p == '\'') { *quote = *p; continue; }
        if (*p == '/' && p[1] == '/') break;
        if (*p == '{') (*depth)++;
        if (*p == '}') (*depth)--;
    }
}

static int is_blank(const char *s) {
    for (; *s; s++) if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') return 0;
    return 1;
}

// Runs `source` in a child. Everything printed before JREPL_MARKER (the replayed history) is dropped.
// Returns 1 when the child compiled and ran to completion.
static int run_in_child(const char *source, int quiet) {
    int fds[2];
    if (pipe(fds) != 0) { perror("pipe"); return 0; }
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 0; }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], 1);
        close(fds[1]);
        if (quiet) {
            int null = open("/dev/null", O_WRONLY);
            if (null >= 0) { dup2(null, 2); close(null); }
        }
        char *ir = jdriver_compile_source("./Repl.ash", source, 0);
        if (!ir) exit(1);
        ashvm_execute_ir(ir);
        exit(0);
    }
    close(fds[1]);
    char *held = NULL;
    size_t held_len = 0;
    int seen = 0;
    char chunk[4096];
    ssize_t n;
    while ((n = read(fds[0], chunk, sizeof(chunk))) > 0) {
        if (seen) { fwrite(chunk, 1, (size_t)n, stdout); continue; }
        held = realloc(held, held_len + (size_t)n + 1);
        memcpy(held + held_len, chunk, (size_t)n);
        held_len += (size_t)n;
        held[held_len] = '\0';
        char *m = strstr(held, JREPL_MARKER "\n");
        if (m) {
            m += strlen(JREPL_MARKER "\n");
            fwrite(m, 1, held_len - (size_t)(m - held), stdout);
            seen = 1;
        }
    }
    close(fds[0]);
    free(held);
    int status = 0;
    waitpid(pid, &status, 0);
    fflush(stdout);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int repl_run(void) {
    JRepl *session = jrepl_new();
    printf("Ash REPL (ashvm) -- Java-like Ash. Imports, types, methods and statements persist.\n");
    printf("A line without a trailing ';' or '}' is evaluated and printed. Type 'exit' to quit.\n");
    printf("State is rebuilt by replaying earlier lines, so file writes and stdin reads repeat.\n\n");

    char buffer[16384];
    for (;;) {
        buffer[0] = '\0';
        int depth = 0, has_content = 0;
        char quote = 0;
        for (;;) {
            printf(depth == 0 ? "ashvm> " : "...    ");
            fflush(stdout);
            char line[2048];
            if (!fgets(line, sizeof(line), stdin)) { printf("\n"); return 0; }
            if (depth == 0 && !has_content && (strcmp(line, "exit\n") == 0 || strcmp(line, "exit") == 0)) return 0;
            if (!is_blank(line)) has_content = 1;
            count_braces(line, &depth, &quote);
            strncat(buffer, line, sizeof(buffer) - strlen(buffer) - 1);
            if (depth <= 0) break;
        }
        if (!has_content) continue;

        JReplKind kind = jrepl_classify(buffer);
        if (kind == JREPL_EXPRESSION) {
            // a bare expression is printed; if that does not compile (a void call), run it as a statement
            if (run_in_child(jrepl_source(session, buffer, kind, 1), 1)) { jrepl_commit(session, buffer, kind, 1); continue; }
            if (run_in_child(jrepl_source(session, buffer, kind, 0), 0)) jrepl_commit(session, buffer, kind, 0);
            continue;
        }
        if (run_in_child(jrepl_source(session, buffer, kind, 0), 0)) jrepl_commit(session, buffer, kind, 0);
    }
}
