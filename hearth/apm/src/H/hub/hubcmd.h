#ifndef APM_HUBCMD_H
#define APM_HUBCMD_H

// Commands that talk to the APMH hub. The first three need a login (apm auth login, or APM_TOKEN) whose token has packages:write;
// they are what a CI pipeline calls after a release. `[owner/repo]` defaults to the `repository: github:owner/repo` of ash.pkg.
//   apm link [owner/repo]     list this repository on the hub (idempotent)
//   apm sync [owner/repo]     make the hub re-read it now (once a minute per package)
//   apm unlink [owner/repo]   remove it from the hub (the GitHub repository is not touched)
//   apm packages              the packages you linked (needs packages:read)
// These two need no login:
//   apm search <words...>     search the hub
//   apm info <owner/repo>     one package: version, license, links, latest tag
// Each takes the project directory like the other commands and returns an exit code.
int cmd_link(const char *project_dir, int argc, char **argv);
int cmd_sync(const char *project_dir, int argc, char **argv);
int cmd_unlink(const char *project_dir, int argc, char **argv);
int cmd_packages(const char *project_dir, int argc, char **argv);
int cmd_search(const char *project_dir, int argc, char **argv);
int cmd_info(const char *project_dir, int argc, char **argv);

#endif
