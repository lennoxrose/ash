#ifndef APM_CLIENT_H
#define APM_CLIENT_H

#include <stddef.h>
#include "H/hub/config.h"
#include "H/hub/json.h"

// One call to the APMH hub API (docs/API.md of the hub): `path` starts with "/api/v1/". `use_token` sends the saved
// bearer token (never to any other host than a->hub). `body` (may be NULL) is a JSON text.
// Returns 1 for a 2xx answer. On anything else returns 0 with a human-readable reason in `err`; `*status` (may be NULL)
// is then the HTTP status or 0 for a transport failure. `*out` (may be NULL) receives the parsed JSON body when there is
// one, also for an error answer, so a caller can look at {"error": "authorization_pending"}; the caller frees it with json_free().
int hub_call(const Auth *a, const char *method, const char *path, int use_token, const char *body, int *status, Json **out, char *err, size_t err_size);

// Percent-encodes `s` for a query string into `out`. Returns 0 when it does not fit.
int hub_urlencode(const char *s, char *out, size_t out_size);

#endif
