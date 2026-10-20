#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "H/hub/http.h"
#include "H/hub/proc.h"

#ifndef ASH_VERSION
#define ASH_VERSION "dev"
#endif

#define MAX_BODY 5000000u

static int is_local_http(const char *url) {
    return strncmp(url, "http://localhost", 16) == 0 || strncmp(url, "http://127.0.0.1", 16) == 0;
}

int http_url_allowed(const char *url) {
    if (strncmp(url, "https://", 8) != 0 && !is_local_http(url)) return 0;
    for (const char *c = url; *c; c++) { // a conservative URL alphabet: nothing that could break out of a config line
        unsigned char ch = (unsigned char)*c;
        int ok = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || strchr(":/._~?&=%+@-", ch);
        if (!ok) return 0;
    }
    return strlen(url) < 1024;
}

// Appends `key = "value"` to a curl config, escaping the two characters that matter inside curl's double quotes.
// Returns 0 when the value holds a control character (never legitimate here) or the buffer is full.
static int cfg_add(char *cfg, size_t cap, size_t *n, const char *key, const char *value) {
    size_t need = strlen(key) + strlen(value) * 2 + 8;
    if (*n + need >= cap) return 0;
    *n += (size_t)snprintf(cfg + *n, cap - *n, "%s = \"", key);
    for (const char *c = value; *c; c++) {
        unsigned char ch = (unsigned char)*c;
        if (ch < 0x20 || ch == 0x7f) return 0;
        if (ch == '"' || ch == '\\') cfg[(*n)++] = '\\';
        cfg[(*n)++] = (char)ch;
    }
    *n += (size_t)snprintf(cfg + *n, cap - *n, "\"\n");
    return 1;
}

int http_request(const char *method, const char *url, const char *bearer, const char *body, int *status, char **out_body, char *err, size_t err_size) {
    *out_body = NULL;
    if (!http_url_allowed(url)) { snprintf(err, err_size, "refusing to use '%s' (the hub must be an https:// URL)", url); return 0; }
    size_t cap = 4096 + (body ? strlen(body) * 2 : 0), n = 0;
    char *cfg = malloc(cap);
    if (!cfg) { snprintf(err, err_size, "out of memory"); return 0; }
    char auth[256] = "", agent[64];
    if (bearer) snprintf(auth, sizeof(auth), "Authorization: Bearer %s", bearer);
    snprintf(agent, sizeof(agent), "User-Agent: apm/%s", ASH_VERSION);
    n += (size_t)snprintf(cfg, cap, "silent\nshow-error\nstderr = \"-\"\nmax-time = 30\nconnect-timeout = 10\nmax-filesize = %u\nproto = \"%s\"\nwrite-out = \"\\n%%{http_code}\"\n",
                          MAX_BODY, is_local_http(url) ? "=http,https" : "=https");
    int ok = cfg_add(cfg, cap, &n, "request", method) && cfg_add(cfg, cap, &n, "url", url) && cfg_add(cfg, cap, &n, "header", agent)
          && cfg_add(cfg, cap, &n, "header", "Accept: application/json");
    if (ok && bearer) ok = cfg_add(cfg, cap, &n, "header", auth);
    if (ok && body) ok = cfg_add(cfg, cap, &n, "header", "Content-Type: application/json") && cfg_add(cfg, cap, &n, "data-binary", body);
    if (!ok) { free(cfg); snprintf(err, err_size, "could not build the request"); return 0; }

    char *argv[] = { "curl", "-q", "-K", "-", NULL }; // -q first: ignore ~/.curlrc
    char *out;
    int rc = proc_run(argv, cfg, n, &out, MAX_BODY + 1024, NULL);
    memset(cfg, 0, cap); // the token was in there
    free(cfg);
    if (rc == 127) { snprintf(err, err_size, "curl is required (apm uses it to talk to the hub) but could not be started"); return 0; }
    if (rc != 0 || !out) {
        char *nl = out ? strchr(out, '\n') : NULL;
        if (nl) *nl = '\0';
        snprintf(err, err_size, "%s", out && out[0] ? out : "could not reach the hub");
        free(out);
        return 0;
    }
    char *last = strrchr(out, '\n'); // write-out put "\n<status>" after the body
    if (!last) { free(out); snprintf(err, err_size, "malformed answer from the hub"); return 0; }
    *status = atoi(last + 1);
    *last = '\0';
    *out_body = out;
    return 1;
}
