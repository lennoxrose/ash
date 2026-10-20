#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "H/hub/client.h"
#include "H/hub/http.h"

int hub_urlencode(const char *s, char *out, size_t out_size) {
    size_t n = 0;
    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
        int plain = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || strchr("-._~", *c);
        if (n + 4 >= out_size) return 0;
        n += (size_t)(plain ? snprintf(out + n, 2, "%c", *c) : snprintf(out + n, 4, "%%%02X", *c));
    }
    out[n] = '\0';
    return 1;
}

int hub_call(const Auth *a, const char *method, const char *path, int use_token, const char *body, int *status, Json **out, char *err, size_t err_size) {
    char url[1400], *text = NULL;
    int code = 0;
    if (status) *status = 0;
    if (out) *out = NULL;
    snprintf(url, sizeof(url), "%s%s", a->hub, path);
    if (use_token && !a->token[0]) { snprintf(err, err_size, "you are not logged in: run 'apm auth login' (or set APM_TOKEN)"); return 0; }
    if (!http_request(method, url, use_token ? a->token : NULL, body, &code, &text, err, err_size)) return 0;
    if (status) *status = code;
    Json *json = json_parse(text);
    free(text);
    if (code >= 200 && code < 300) {
        if (out) *out = json; else json_free(json);
        return 1;
    }
    const char *message = json_gets(json, "message");
    if (code == 401) snprintf(err, err_size, "the hub did not accept the token (missing, expired or revoked): run 'apm auth login'");
    else snprintf(err, err_size, "%s (HTTP %d)", message ? message : json_gets(json, "error") ? json_gets(json, "error") : "request failed", code);
    if (out) *out = json; else json_free(json);
    return 0;
}
