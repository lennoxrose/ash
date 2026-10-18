#ifndef PYRE_DASHBOARD_H
#define PYRE_DASHBOARD_H
// "ash" by alphabet position: a=1, s=19, h=8.
#define PYRE_DASHBOARD_PORT 1198

// `pyre dashboard [--port N] [--no-open]`: serves the built web UI
// (bin/extensions/pyre/web) and a small JSON API on 127.0.0.1 only, fully
// offline. Returns the process exit code.
int pyre_dashboard_main(int argc, char **argv);
#endif
