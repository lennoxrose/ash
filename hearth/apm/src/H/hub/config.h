#ifndef APM_CONFIG_H
#define APM_CONFIG_H

// Where apm keeps its login. One hub, one token, in <config dir>/hosts (mode 0600, directory 0700):
//   hub: https://hub.ash-project.org     token: apm:...     login: lennoxrose     expires: 2027-01-18T10:00:00.000Z
// Config dir: $APM_CONFIG_DIR, else $XDG_CONFIG_HOME/apm, else ~/.config/apm.
// Overrides for scripts and CI: APM_TOKEN (the token, nothing is read from or written to disk for it) and APM_HUB (the hub URL).
#define DEFAULT_HUB "https://hub.ash-project.org"
#define TOKEN_LEN 128
#define HUB_LEN 256

typedef struct {
    char hub[HUB_LEN];
    char token[TOKEN_LEN]; // "" when logged out
    char login[64];
    char expires[40];
    int from_env;          // the token came from APM_TOKEN
} Auth;

// Fills `a` from the environment and the saved file. Always sets a usable hub. Returns 1 when a token is present.
int config_load(Auth *a);
// Saves hub/token/login/expires (0600). Returns 1 on success.
int config_save(const Auth *a);
// Removes the saved login. Returns 1 when something was removed.
int config_clear(void);
// Path of the hosts file (for messages).
const char *config_path(void);

// "apm:" followed by 86 base64url characters, the shape of every token the hub issues.
int token_looks_valid(const char *token);
// Normalizes a hub URL in place (drops a trailing '/') and checks it with http_url_allowed(). Returns 1 when usable.
int hub_url_normalize(char *url);

#endif
