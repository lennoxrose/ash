#ifndef APM_AUTH_H
#define APM_AUTH_H

// apm auth login [--web | --with-token] [--hub <url>] [--name <label>] [--no-browser]
// apm auth status | logout | token
//
// Like `gh auth login`: with no flags on a terminal it asks how to log in. --web runs the hub's device flow (apm shows a
// one-time code, you approve it in the browser, apm receives a token valid for 90 days). --with-token reads a token you
// made yourself in Settings > API tokens from stdin. The token is kept in <config dir>/hosts with mode 0600 (see config.h)
// and APM_TOKEN overrides it, which is what CI uses. Returns an exit code.
int cmd_auth(int argc, char **argv);


#endif
