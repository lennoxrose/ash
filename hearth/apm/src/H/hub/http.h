#ifndef APM_HTTP_H
#define APM_HTTP_H

#include <stddef.h>

// One HTTPS (or, for localhost development, HTTP) request, made by running the system `curl` -- apm has no
// TLS stack of its own and, like for git, shells out to the standard tool. curl runs without a shell and without
// the user's ~/.curlrc, and its whole configuration, including the bearer token, goes through its stdin: the token
// never appears in the process list or on disk. Redirects are not followed, the transfer is capped at 30 s and 5 MB.
//
// `bearer` and `body` may be NULL; a body is sent as application/json. Returns 1 when an HTTP answer was received
// (any status; *status and *out_body are set, the caller frees *out_body), 0 on a transport failure (curl missing,
// no network, bad URL; a one-line reason is written to `err`).
int http_request(const char *method, const char *url, const char *bearer, const char *body, int *status, char **out_body, char *err, size_t err_size);

// Plain http:// is only accepted for localhost / 127.0.0.1 (a developer's own hub); everything else must be https://.
int http_url_allowed(const char *url);

#endif
