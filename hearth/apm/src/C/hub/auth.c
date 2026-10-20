#define _XOPEN_SOURCE 700 // gethostname, termios, sleep under -std=c11
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "H/hub/auth.h"
#include "H/hub/client.h"
#include "H/hub/proc.h"
#include "H/ui.h"

static void device_name(char *out, size_t size) {
    char host[128] = "";
    if (gethostname(host, sizeof(host) - 1) != 0) host[0] = '\0';
    size_t n = 0;
    for (const char *c = host; *c && n < 40 && n + 1 < size; c++) {
        int ok = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || strchr(" ._@-", *c);
        out[n++] = ok ? *c : '-';
    }
    out[n] = '\0';
    if (n == 0) snprintf(out, size, "unknown machine");
}

static void open_browser(const char *url) {
#ifdef __APPLE__
    char *argv[] = { "open", (char *)url, NULL };
    int can = 1;
#else
    char *argv[] = { "xdg-open", (char *)url, NULL };
    int can = getenv("DISPLAY") != NULL || getenv("WAYLAND_DISPLAY") != NULL;
#endif
    if (can && isatty(1)) proc_quiet(argv);
}

static int save_login(Auth *a, const char *token, const char *login, const char *expires) {
    snprintf(a->token, sizeof(a->token), "%s", token);
    snprintf(a->login, sizeof(a->login), "%s", login ? login : "");
    snprintf(a->expires, sizeof(a->expires), "%s", expires ? expires : "");
    if (!config_save(a)) { ui_err("could not save the login to %s", config_path()); return 1; }
    ui_ok("logged in to %s as %s%s%.10s%s", a->hub, login ? login : "?", expires ? " (token expires " : "", expires ? expires : "", expires ? ")" : "");
    return 0;
}

// The device flow: ask the hub for a code, wait while the person approves it in the browser, receive the token.
static int login_web(Auth *a, const char *name, int browser) {
    char err[512], body[128];
    snprintf(body, sizeof(body), "{\"name\":\"%s\"}", name);
    Json *start = NULL;
    if (!hub_call(a, "POST", "/api/v1/auth/device", 0, body, NULL, &start, err, sizeof(err))) { ui_err("%s", err); json_free(start); return 1; }
    const char *device = json_gets(start, "device_code"), *user = json_gets(start, "user_code"), *uri = json_gets(start, "verification_uri");
    const Json *iv = json_get(start, "interval"), *ex = json_get(start, "expires_in");
    if (!device || !user || !uri) { ui_err("the hub sent an unexpected answer"); json_free(start); return 1; }
    int interval = (iv && iv->type == J_NUM && iv->num >= 1 && iv->num <= 60) ? (int)iv->num : 5;
    time_t deadline = time(NULL) + ((ex && ex->type == J_NUM && ex->num > 0 && ex->num <= 3600) ? (int)ex->num : 600);
    char poll_body[160];
    snprintf(poll_body, sizeof(poll_body), "{\"device_code\":\"%s\"}", device);
    ui_header("First copy your one-time code: %s", user);
    ui_info("then open %s and enter it", uri);
    if (browser) open_browser(uri);
    ui_info("waiting for you to approve it in the browser...");
    json_free(start);

    while (time(NULL) < deadline) {
        sleep((unsigned)interval);
        Json *res = NULL;
        if (hub_call(a, "POST", "/api/v1/auth/device/token", 0, poll_body, NULL, &res, err, sizeof(err))) {
            const char *token = json_gets(res, "token");
            int bad = !token || !token_looks_valid(token);
            int rc = bad ? (ui_err("the hub sent an unexpected token"), 1) : save_login(a, token, json_gets(res, "login"), json_gets(res, "expiresAt"));
            json_free(res);
            return rc;
        }
        const char *code = json_gets(res, "error");
        int pending = code && strcmp(code, "authorization_pending") == 0, slow = code && strcmp(code, "slow_down") == 0;
        if (slow) interval = interval + 5 > 60 ? 60 : interval + 5;
        if (!pending && !slow) { ui_err("%s", code && strcmp(code, "expired_token") == 0 ? "the login timed out or was denied, run apm auth login again" : err); json_free(res); return 1; }
        json_free(res);
    }
    ui_err("the one-time code expired, run apm auth login again");
    return 1;
}

static int read_secret(char *out, size_t size) {
    struct termios old, quiet;
    int tty = isatty(0) && tcgetattr(0, &old) == 0;
    if (tty) {
        fputs("Paste your token (Settings > API tokens on the hub): ", stderr);
        quiet = old;
        quiet.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(0, TCSANOW, &quiet);
    }
    char *got = fgets(out, (int)size, stdin);
    if (tty) { tcsetattr(0, TCSANOW, &old); fputc('\n', stderr); }
    if (!got) return 0;
    out[strcspn(out, "\r\n")] = '\0';
    return 1;
}

static int login_token(Auth *a) {
    char token[TOKEN_LEN], err[512];
    if (!read_secret(token, sizeof(token)) || !token_looks_valid(token)) { ui_err("that is not an apm token (it looks like apm:… and is 90 characters long)"); return 1; }
    snprintf(a->token, sizeof(a->token), "%s", token);
    Json *info = NULL;
    if (!hub_call(a, "GET", "/api/v1/me/token", 1, NULL, NULL, &info, err, sizeof(err))) { ui_err("%s", err); json_free(info); return 1; }
    const Json *t = json_get(info, "token");
    int rc = save_login(a, token, json_gets(t, "login"), json_gets(t, "expiresAt"));
    json_free(info);
    return rc;
}

static int login(int argc, char **argv) {
    Auth a;
    config_load(&a);
    int web = 0, with_token = 0, browser = 1;
    char name[64];
    device_name(name, sizeof(name));
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--web") == 0) web = 1;
        else if (strcmp(argv[i], "--with-token") == 0) with_token = 1;
        else if (strcmp(argv[i], "--no-browser") == 0) browser = 0;
        else if (strcmp(argv[i], "--hub") == 0 && i + 1 < argc) {
            snprintf(a.hub, sizeof(a.hub), "%s", argv[++i]);
            if (!hub_url_normalize(a.hub)) { ui_err("the hub must be an https:// URL"); return 1; }
        } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) snprintf(name, sizeof(name), "%.40s", argv[++i]);
        else { ui_err("usage: apm auth login [--web | --with-token] [--hub <url>] [--name <label>] [--no-browser]"); return 1; }
    }
    if (web && with_token) { ui_err("choose one of --web and --with-token"); return 1; }
    if (!web && !with_token) {
        if (!isatty(0)) { ui_err("not a terminal: pass --web or --with-token"); return 1; }
        char answer[16] = "";
        printf("How would you like to log in to %s?\n  1) Login with a web browser\n  2) Paste an authentication token\n? ", a.hub);
        if (!fgets(answer, sizeof(answer), stdin)) return 1;
        with_token = answer[0] == '2';
        web = !with_token;
    }
    a.token[0] = '\0'; // never carry an earlier login (or an APM_TOKEN) into the request
    return with_token ? login_token(&a) : login_web(&a, name, browser);
}

static int status(void) {
    Auth a;
    char err[512];
    if (!config_load(&a)) { ui_err("not logged in to %s: run 'apm auth login'", a.hub); return 1; }
    Json *info = NULL;
    if (!hub_call(&a, "GET", "/api/v1/me/token", 1, NULL, NULL, &info, err, sizeof(err))) { ui_err("%s: %s", a.hub, err); json_free(info); return 1; }
    const Json *t = json_get(info, "token"), *scopes = json_get(t, "scopes");
    const char *expires = json_gets(t, "expiresAt");
    ui_ok("logged in to %s as %s (%s)", a.hub, json_gets(t, "login"), a.from_env ? "token from APM_TOKEN" : config_path());
    ui_bullet("token: %s (apm:...%s)", json_gets(t, "name"), json_gets(t, "hint"));
    printf("\xe2\x80\xa2 permissions:");
    for (size_t i = 0; scopes && i < scopes->count; i++) printf(" %s", json_str(json_at(scopes, i)));
    printf("\n");
    ui_bullet("expires: %.10s", expires ? expires : "never");
    json_free(info);
    return 0;
}

int cmd_auth(int argc, char **argv) {
    if (argc < 1) { ui_err("usage: apm auth login | status | logout | token"); return 1; }
    if (strcmp(argv[0], "login") == 0) return login(argc - 1, argv + 1);
    if (strcmp(argv[0], "status") == 0) return status();
    if (strcmp(argv[0], "token") == 0) {
        Auth a;
        if (!config_load(&a)) { ui_err("not logged in: run 'apm auth login'"); return 1; }
        printf("%s\n", a.token);
        return 0;
    }
    if (strcmp(argv[0], "logout") == 0) {
        Auth a;
        int had_env = getenv("APM_TOKEN") != NULL;
        config_load(&a);
        int removed = config_clear();
        if (!removed && !had_env) { ui_info("not logged in"); return 0; }
        if (removed) ui_ok("logged out (the token stays valid until it expires: revoke it at %s/settings/tokens)", a.hub);
        if (had_env) ui_info("APM_TOKEN is set in your environment and still applies: unset it too");
        return 0;
    }
    ui_err("unknown 'apm auth' command '%s' (login, status, logout, token)", argv[0]);
    return 1;
}
