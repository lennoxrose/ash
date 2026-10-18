#ifndef APM_CMD_H
#define APM_CMD_H

// Each command takes the project directory ("." almost always, since
// apm is always run from a project's root) and the remaining argv
// past the subcommand name. Returns an exit code.
int cmd_init(const char *project_dir, int argc, char **argv);
int cmd_add(const char *project_dir, int argc, char **argv);
int cmd_install(const char *project_dir, int argc, char **argv);
int cmd_list(const char *project_dir, int argc, char **argv);
int cmd_run(const char *project_dir, int argc, char **argv);

#endif
