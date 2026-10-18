/* `pyre dashboard`: a tiny local web server for the Pyre dashboard.
 *
 * Fully offline and local: binds 127.0.0.1 only, serves the pre-built static
 * UI from <directory of this executable>/web, and exposes a small API:
 *
 *   GET  /api/state            what is learned (runs `pyre info --json`)
 *   GET  /api/history          finished train/check sessions (history.jsonl)
 *   GET  /api/stamp            when the profile / history files last changed (cheap: lets the page
 *                              refresh itself only when something actually changed)
 *   POST /api/job              start a session: {"cmd":"train|check|forget","args":[...]}
 *   GET  /api/job?since=N      live status + the session's JSON events from N on
 *   POST /api/job/stop         stop the running session
 *
 * The port is always PYRE_DASHBOARD_PORT (1198: a=1, s=19, h=8, "ash" by alphabet
 * position) unless --port says otherwise; it never hops to another port on its own.
 *
 * Every session is a child `pyre <cmd> ... --json`, so the dashboard runs
 * exactly the same code as the command line and cannot drift from it. One
 * session at a time. Single-threaded: a poll() loop serves requests and
 * drains the running session's output.
 */
#define _POSIX_C_SOURCE 200809L
#include "dashboard.h"
#include "H/runtime/manager.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINES 60000
#define REQ_MAX 20000

static char self_path[1024];
static char web_dir[1100];
static int port;
static volatile sig_atomic_t stop_requested;

/* ---------- the running session ---------- */

static struct {
    pid_t pid;
    int fd;                 /* read end of the child's stdout+stderr, -1 when not running */
    int running;
    int exit_code;          /* valid once !running and pid != 0 */
    char cmd[16];
    long long started;
    char **lines;           /* one JSON object per line (the newest `count`) */
    int count;
    int dropped;            /* lines discarded from the front; indices stay absolute */
    char partial[8192];
    int partial_len;
} job = {.fd = -1};

static void job_clear_lines(void) {
    for (int i = 0; i < job.count; i++) free(job.lines[i]);
    free(job.lines);
    job.lines = NULL;
    job.count = 0;
    job.dropped = 0;
    job.partial_len = 0;
}

static void job_add_line(const char *line) {
    if (job.count >= MAX_LINES) {  /* a session that never ends must not grow without bound: drop the oldest half */
        int half = MAX_LINES / 2;
        for (int i = 0; i < half; i++) free(job.lines[i]);
        memmove(job.lines, job.lines + half, sizeof(char *) * (size_t)(job.count - half));
        job.count -= half;
        job.dropped += half;
    }
    char *copy;
    if (line[0] == '{') {
        copy = strdup(line);
    } else {  /* plain text from the child (an error, say): wrap as a log event */
        size_t n = strlen(line);
        copy = malloc(n * 6 + 40);
        if (!copy) return;
        char *w = copy + sprintf(copy, "{\"event\":\"log\",\"message\":\"");
        for (const char *r = line; *r; r++) {
            unsigned char c = (unsigned char)*r;
            if (c == '"' || c == '\\') { *w++ = '\\'; *w++ = (char)c; }
            else if (c < 0x20) w += sprintf(w, "\\u%04x", c);
            else *w++ = (char)c;
        }
        strcpy(w, "\"}");
    }
    if (!copy) return;
    char **grown = realloc(job.lines, sizeof(char *) * (size_t)(job.count + 1));
    if (!grown) { free(copy); return; }
    job.lines = grown;
    job.lines[job.count++] = copy;
}

static void job_drain(void) {
    char buf[4096];
    ssize_t n;
    while ((n = read(job.fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                job.partial[job.partial_len] = '\0';
                if (job.partial_len) job_add_line(job.partial);
                job.partial_len = 0;
            } else if (job.partial_len < (int)sizeof(job.partial) - 1) {
                job.partial[job.partial_len++] = buf[i];
            }
        }
    }
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {  /* child closed its output */
        int status = 0;
        waitpid(job.pid, &status, 0);
        job.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        close(job.fd);
        job.fd = -1;
        job.running = 0;
    }
}

/* argv for the child must be plain words: no shell is ever involved, and the
 * child (pyre itself) validates every flag again. */
static int word_ok(const char *w) {
    if (!*w || strlen(w) > 64) return 0;
    for (; *w; w++) {
        if (!((*w >= 'a' && *w <= 'z') || (*w >= 'A' && *w <= 'Z') || (*w >= '0' && *w <= '9') ||
              *w == '-' || *w == '_' || *w == '.' || *w == ',')) return 0;
    }
    return 1;
}

static int job_start(const char *cmd, char **args, int nargs) {
    if (job.running) return 0;
    job_clear_lines();
    int fds[2];
    if (pipe(fds) != 0) return 0;
    char *argv[40];
    int n = 0;
    argv[n++] = self_path;
    argv[n++] = (char *)cmd;
    for (int i = 0; i < nargs && n < 36; i++) argv[n++] = args[i];
    argv[n++] = "--json";
    argv[n] = NULL;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return 0; }
    if (pid == 0) {
        dup2(fds[1], 1);
        dup2(fds[1], 2);
        close(fds[0]);
        close(fds[1]);
        execv(self_path, argv);
        _exit(127);
    }
    close(fds[1]);
    fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);
    job.pid = pid;
    job.fd = fds[0];
    job.running = 1;
    job.exit_code = 0;
    job.started = (long long)time(NULL);
    snprintf(job.cmd, sizeof(job.cmd), "%s", cmd);
    return 1;
}

/* ---------- HTTP ---------- */

static void send_all(int fd, const char *data, size_t len) {
    while (len) {
        ssize_t n = send(fd, data, len, MSG_NOSIGNAL);
        if (n <= 0) return;
        data += n;
        len -= (size_t)n;
    }
}

static void respond(int fd, int status, const char *type, const char *body, size_t len) {
    const char *reason = status == 200 ? "OK" : status == 400 ? "Bad Request" : status == 403 ? "Forbidden" :
                         status == 404 ? "Not Found" : status == 409 ? "Conflict" : "Service Unavailable";
    char head[256];
    int h = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\n"
                     "X-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n", status, reason, type, len);
    send_all(fd, head, (size_t)h);
    send_all(fd, body, len);
}

static void respond_text(int fd, int status, const char *type, const char *body) { respond(fd, status, type, body, strlen(body)); }

typedef struct { char *data; size_t len, cap; } Buf;
static void buf_add(Buf *b, const char *s, size_t n) {
    if (n > (size_t)1 << 30) return;
    if (b->len + n + 1 > b->cap) {
        b->cap = (b->len + n + 1) * 2;
        b->data = realloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

/* Runs `pyre <args> --json` to completion and returns its first stdout line. */
static char *run_capture(const char *cmd) {
    int fds[2];
    if (pipe(fds) != 0) return NULL;
    pid_t pid = fork();
    if (pid == 0) {
        dup2(fds[1], 1);
        close(fds[0]);
        close(fds[1]);
        execl(self_path, self_path, cmd, "--json", (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    Buf b = {0};
    char chunk[4096];
    ssize_t n;
    while ((n = read(fds[0], chunk, sizeof(chunk))) > 0) buf_add(&b, chunk, (size_t)n);
    close(fds[0]);
    waitpid(pid, NULL, 0);
    return b.data;
}

static void history_path(char *out, size_t cap) {
    const char *prof = pyre_manager_profile_path();
    snprintf(out, cap, "%s", prof);
    char *slash = strrchr(out, '/');
    if (!*prof) { out[0] = '\0'; return; }
    snprintf(slash ? slash + 1 : out, cap - (slash ? (size_t)(slash + 1 - out) : 0), "history.jsonl");
}

static void api_history(int fd) {
    char path[1200];
    history_path(path, sizeof(path));
    Buf b = {0};
    buf_add(&b, "[", 1);
    FILE *f = path[0] ? fopen(path, "r") : NULL;
    if (f) {
        char line[4096];
        int first = 1;
        while (fgets(line, sizeof(line), f)) {
            size_t n = strcspn(line, "\r\n");
            if (!n || line[0] != '{') continue;
            if (!first) buf_add(&b, ",", 1);
            buf_add(&b, line, n);
            first = 0;
        }
        fclose(f);
    }
    buf_add(&b, "]", 1);
    respond(fd, 200, "application/json", b.data, b.len);
    free(b.data);
}

/* Modification time of a file in nanoseconds (0 if it does not exist). */
static long long mtime_ns(const char *path) {
    struct stat st;
    if (!path[0] || stat(path, &st) != 0) return 0;
    return (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
}

static void api_stamp(int fd) {
    char hist[1200], out[160];
    history_path(hist, sizeof(hist));
    int n = snprintf(out, sizeof(out), "{\"profile\":%lld,\"history\":%lld}", mtime_ns(pyre_manager_profile_path()), mtime_ns(hist));
    respond(fd, 200, "application/json", out, (size_t)n);
}

static void api_job_status(int fd, int since) {
    if (since < 0) since = 0;
    Buf b = {0};
    char head[256];
    int h = snprintf(head, sizeof(head), "{\"running\":%s,\"cmd\":\"%s\",\"started\":%lld,\"exit\":",
                     job.running ? "true" : "false", job.cmd, job.started);
    buf_add(&b, head, (size_t)h);
    if (!job.running && job.pid) { h = snprintf(head, sizeof(head), "%d", job.exit_code); buf_add(&b, head, (size_t)h); }
    else buf_add(&b, "null", 4);
    h = snprintf(head, sizeof(head), ",\"next\":%d,\"lines\":[", job.dropped + job.count);
    buf_add(&b, head, (size_t)h);
    int from = since > job.dropped ? since - job.dropped : 0;  /* `since` is an absolute index */
    for (int i = from; i < job.count; i++) {
        if (i > from) buf_add(&b, ",", 1);
        buf_add(&b, job.lines[i], strlen(job.lines[i]));
    }
    buf_add(&b, "]}", 2);
    respond(fd, 200, "application/json", b.data, b.len);
    free(b.data);
}

/* Minimal reader for {"cmd":"x","args":["a","b"]}. */
static int parse_job_body(const char *body, char *cmd, size_t cmd_cap, char **args, int *nargs, char *store, size_t store_cap) {
    const char *c = strstr(body, "\"cmd\"");
    if (!c || !(c = strchr(c + 5, ':'))) return 0;
    c = strchr(c, '"');
    if (!c) return 0;
    c++;
    size_t n = strcspn(c, "\"");
    if (n == 0 || n >= cmd_cap) return 0;
    memcpy(cmd, c, n);
    cmd[n] = '\0';
    *nargs = 0;
    const char *a = strstr(body, "\"args\"");
    if (!a) return 1;
    a = strchr(a, '[');
    if (!a) return 0;
    a++;
    size_t used = 0;
    while (*a && *a != ']') {
        if (*a == '"') {
            a++;
            size_t len = strcspn(a, "\"");
            if (!a[len] || *nargs >= 32 || used + len + 1 > store_cap) return 0;
            memcpy(store + used, a, len);
            store[used + len] = '\0';
            args[(*nargs)++] = store + used;
            used += len + 1;
            a += len + 1;
        } else a++;
    }
    return 1;
}

static void api_job_start(int fd, const char *body) {
    char cmd[16], store[1024];
    char *args[32];
    int nargs;
    if (!parse_job_body(body, cmd, sizeof(cmd), args, &nargs, store, sizeof(store))) { respond_text(fd, 400, "text/plain", "bad request body"); return; }
    if (strcmp(cmd, "train") && strcmp(cmd, "check") && strcmp(cmd, "forget")) { respond_text(fd, 400, "text/plain", "unknown command"); return; }
    for (int i = 0; i < nargs; i++) if (!word_ok(args[i])) { respond_text(fd, 400, "text/plain", "bad argument"); return; }
    if (!job_start(cmd, args, nargs)) { respond_text(fd, 409, "text/plain", "a session is already running"); return; }
    respond_text(fd, 200, "application/json", "{\"started\":true}");
}

static const char *mime_of(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    static const struct { const char *ext, *type; } types[] = {
        {".html", "text/html; charset=utf-8"}, {".js", "text/javascript; charset=utf-8"}, {".css", "text/css; charset=utf-8"},
        {".svg", "image/svg+xml"}, {".json", "application/json"}, {".png", "image/png"}, {".ico", "image/x-icon"},
        {".woff2", "font/woff2"}, {".map", "application/json"}, {".txt", "text/plain; charset=utf-8"}};
    for (unsigned i = 0; i < sizeof(types) / sizeof(types[0]); i++) if (!strcmp(dot, types[i].ext)) return types[i].type;
    return "application/octet-stream";
}

static void serve_static(int fd, const char *url_path) {
    char rel[640];
    snprintf(rel, sizeof(rel), "%s", url_path);
    rel[strcspn(rel, "?#")] = '\0';
    if (strstr(rel, "..") || strchr(rel, '\\')) { respond_text(fd, 403, "text/plain", "forbidden"); return; }
    if (!strcmp(rel, "/")) snprintf(rel, sizeof(rel), "/index.html");
    char full[2000];
    snprintf(full, sizeof(full), "%s%s", web_dir, rel);
    struct stat st;
    if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) {
        if (strchr(rel + 1, '.')) { respond_text(fd, 404, "text/plain", "not found"); return; }
        snprintf(full, sizeof(full), "%s/index.html", web_dir);  /* single-page app fallback */
        if (stat(full, &st) != 0) {
            respond_text(fd, 503, "text/html; charset=utf-8",
                         "<h1>Pyre dashboard is not built</h1><p>Run <code>make web</code> in <code>pyre/</code> (needs Node and pnpm once, at build time).</p>");
            return;
        }
    }
    FILE *f = fopen(full, "rb");
    if (!f) { respond_text(fd, 404, "text/plain", "not found"); return; }
    char *data = malloc((size_t)st.st_size + 1);
    size_t got = data ? fread(data, 1, (size_t)st.st_size, f) : 0;
    fclose(f);
    if (data) respond(fd, 200, mime_of(full), data, got);
    free(data);
}

static const char *header_value(const char *req, const char *name, char *out, size_t cap) {
    size_t nl = strlen(name);
    for (const char *p = req; (p = strstr(p, "\r\n")); ) {
        p += 2;
        if (!strncasecmp(p, name, nl) && p[nl] == ':') {
            p += nl + 1;
            while (*p == ' ') p++;
            size_t n = strcspn(p, "\r\n");
            if (n >= cap) n = cap - 1;
            memcpy(out, p, n);
            out[n] = '\0';
            return out;
        }
    }
    return NULL;
}

static void handle_client(int fd) {
    struct timeval tv = {2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    char req[REQ_MAX + 1];
    size_t len = 0;
    char *body = NULL;
    while (len < REQ_MAX) {
        ssize_t n = recv(fd, req + len, REQ_MAX - len, 0);
        if (n <= 0) break;
        len += (size_t)n;
        req[len] = '\0';
        body = strstr(req, "\r\n\r\n");
        if (body) {
            char cl[32];
            size_t want = header_value(req, "Content-Length", cl, sizeof(cl)) ? (size_t)atoi(cl) : 0;
            if (len >= (size_t)(body + 4 - req) + want) break;
        }
    }
    req[len] = '\0';
    if (!body) { respond_text(fd, 400, "text/plain", "bad request"); return; }
    body += 4;

    char method[8], target[600];
    if (sscanf(req, "%7s %599s", method, target) != 2) { respond_text(fd, 400, "text/plain", "bad request"); return; }

    /* Local-only: refuse requests whose Host is not us (DNS rebinding) and
     * cross-site POSTs (a web page you visit cannot drive training). */
    char host[128], expect_a[64], expect_b[64], origin[160];
    snprintf(expect_a, sizeof(expect_a), "127.0.0.1:%d", port);
    snprintf(expect_b, sizeof(expect_b), "localhost:%d", port);
    if (!header_value(req, "Host", host, sizeof(host)) || (strcmp(host, expect_a) && strcmp(host, expect_b))) {
        respond_text(fd, 403, "text/plain", "forbidden host");
        return;
    }
    if (!strcmp(method, "POST") && header_value(req, "Origin", origin, sizeof(origin))) {
        char ok_a[96], ok_b[96];
        snprintf(ok_a, sizeof(ok_a), "http://%s", expect_a);
        snprintf(ok_b, sizeof(ok_b), "http://%s", expect_b);
        if (strcmp(origin, ok_a) && strcmp(origin, ok_b)) { respond_text(fd, 403, "text/plain", "forbidden origin"); return; }
    }

    if (!strcmp(method, "GET") && !strcmp(target, "/api/state")) {
        char *out = run_capture("info");
        respond_text(fd, out ? 200 : 503, "application/json", out ? out : "{}");
        free(out);
    } else if (!strcmp(method, "GET") && !strcmp(target, "/api/history")) {
        api_history(fd);
    } else if (!strcmp(method, "GET") && !strcmp(target, "/api/stamp")) {
        api_stamp(fd);
    } else if (!strcmp(method, "GET") && !strncmp(target, "/api/job", 8)) {
        const char *q = strstr(target, "since=");
        api_job_status(fd, q ? atoi(q + 6) : 0);
    } else if (!strcmp(method, "POST") && !strcmp(target, "/api/job")) {
        api_job_start(fd, body);
    } else if (!strcmp(method, "POST") && !strcmp(target, "/api/job/stop")) {
        if (job.running) kill(job.pid, SIGTERM);
        respond_text(fd, 200, "application/json", "{\"stopping\":true}");
    } else if (!strcmp(method, "GET")) {
        serve_static(fd, target);
    } else {
        respond_text(fd, 404, "text/plain", "not found");
    }
}

static void on_signal(int sig) { (void)sig; stop_requested = 1; }

int pyre_dashboard_main(int argc, char **argv) {
    int want_port = PYRE_DASHBOARD_PORT, open_browser = 1;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc) want_port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--no-open")) open_browser = 0;
        else { fprintf(stderr, "pyre dashboard: unknown option '%s' (see `pyre help dashboard`)\n", argv[i]); return 2; }
    }
    ssize_t n = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (n <= 0) { fprintf(stderr, "pyre dashboard: cannot locate this executable\n"); return 1; }
    self_path[n] = '\0';
    snprintf(web_dir, sizeof(web_dir), "%s", self_path);
    *strrchr(web_dir, '/') = '\0';
    strncat(web_dir, "/web", sizeof(web_dir) - strlen(web_dir) - 1);

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    port = want_port;
    addr.sin_port = htons((uint16_t)port);
    if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(lfd, 8) != 0) {
        fprintf(stderr, "pyre dashboard: cannot listen on 127.0.0.1:%d (%s).%s\n", port, strerror(errno),
                errno == EADDRINUSE ? " Is a dashboard already running? Open http://127.0.0.1:1198 or pass --port N." : "");
        return 1;
    }

    struct sigaction sa = {.sa_handler = on_signal};
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    struct stat st;
    char index_path[1200];
    snprintf(index_path, sizeof(index_path), "%s/index.html", web_dir);
    if (stat(index_path, &st) != 0) fprintf(stderr, "pyre dashboard: warning: %s is missing -- run `make web` in pyre/\n", web_dir);

    printf("Pyre dashboard: http://127.0.0.1:%d  (offline, local only; Ctrl+C to stop)\n", port);
    fflush(stdout);
    if (open_browser && fork() == 0) {
        char url[64];
        snprintf(url, sizeof(url), "http://127.0.0.1:%d", port);
        int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2);
        execlp("xdg-open", "xdg-open", url, (char *)NULL);
        _exit(0);
    }

    while (!stop_requested) {
        struct pollfd fds[2] = {{lfd, POLLIN, 0}, {job.fd, POLLIN, 0}};
        int nfds = job.running ? 2 : 1;
        if (poll(fds, (nfds_t)nfds, 500) < 0 && errno != EINTR) break;
        if (job.running && (fds[1].revents & (POLLIN | POLLHUP | POLLERR))) job_drain();
        if (fds[0].revents & POLLIN) {
            int cfd = accept(lfd, NULL, NULL);
            if (cfd >= 0) { handle_client(cfd); close(cfd); }
        }
    }
    if (job.running) { kill(job.pid, SIGTERM); waitpid(job.pid, NULL, 0); }
    close(lfd);
    return 0;
}
