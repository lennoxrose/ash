#define _XOPEN_SOURCE 700 // mkdir/rename/fileno under -std=c11
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "H/hub/config.h"
#include "H/hub/http.h"
#include "H/fsutil.h"

static char hosts_path[1100];
static char config_dir[1024];

static void resolve_dir(void) {
    const char *over = getenv("APM_CONFIG_DIR"), *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (over && over[0]) snprintf(config_dir, sizeof(config_dir), "%s", over);
    else if (xdg && xdg[0]) snprintf(config_dir, sizeof(config_dir), "%s/apm", xdg);
    else snprintf(config_dir, sizeof(config_dir), "%s/.config/apm", home && home[0] ? home : ".");
    snprintf(hosts_path, sizeof(hosts_path), "%s/hosts", config_dir);
}

const char *config_path(void) { resolve_dir(); return hosts_path; }

int token_looks_valid(const char *t) {
    if (strncmp(t, "apm:", 4) != 0 || strlen(t) != 90) return 0;
    for (const char *c = t + 4; *c; c++) {
        int ok = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '_';
        if (!ok) return 0;
    }
    return 1;
}

int hub_url_normalize(char *url) {
    size_t n = strlen(url);
    while (n > 0 && url[n - 1] == '/') url[--n] = '\0';
    return n > 0 && http_url_allowed(url);
}

static void put(char *dst, size_t size, const char *value) { snprintf(dst, size, "%s", value); }

int config_load(Auth *a) {
    memset(a, 0, sizeof(*a));
    put(a->hub, sizeof(a->hub), DEFAULT_HUB);
    resolve_dir();
    FILE *f = fopen(hosts_path, "r");
    char line[512];
    while (f && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *value = colon + 1;
        while (*value == ' ') value++;
        if (strcmp(line, "hub") == 0) put(a->hub, sizeof(a->hub), value);
        else if (strcmp(line, "token") == 0) put(a->token, sizeof(a->token), value);
        else if (strcmp(line, "login") == 0) put(a->login, sizeof(a->login), value);
        else if (strcmp(line, "expires") == 0) put(a->expires, sizeof(a->expires), value);
    }
    if (f) fclose(f);
    if (!hub_url_normalize(a->hub)) put(a->hub, sizeof(a->hub), DEFAULT_HUB);
    const char *hub = getenv("APM_HUB"), *token = getenv("APM_TOKEN");
    if (hub && hub[0]) {
        char wanted[HUB_LEN];
        put(wanted, sizeof(wanted), hub);
        if (hub_url_normalize(wanted) && strcmp(wanted, a->hub) != 0) {
            // A saved token belongs to the hub it was issued by: never send it anywhere else.
            put(a->hub, sizeof(a->hub), wanted);
            a->token[0] = a->login[0] = a->expires[0] = '\0';
        }
    }
    if (token && token[0]) { put(a->token, sizeof(a->token), token); a->from_env = 1; a->login[0] = '\0'; a->expires[0] = '\0'; }
    return a->token[0] != '\0';
}

int config_save(const Auth *a) {
    resolve_dir();
    char tmp[1200];
    snprintf(tmp, sizeof(tmp), "%s.tmp", hosts_path);
    if (!fsutil_mkdir_p(config_dir)) return 0;
    chmod(config_dir, 0700);
    unlink(tmp);
    FILE *f = fopen(tmp, "w");
    if (!f) return 0;
    chmod(tmp, 0600); // before the secret goes in
    fprintf(f, "hub: %s\ntoken: %s\nlogin: %s\nexpires: %s\n", a->hub, a->token, a->login, a->expires);
    int ok = fclose(f) == 0 && rename(tmp, hosts_path) == 0;
    if (!ok) unlink(tmp);
    return ok;
}

int config_clear(void) {
    resolve_dir();
    return unlink(hosts_path) == 0;
}
