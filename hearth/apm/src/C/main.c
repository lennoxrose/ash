#include <stdio.h>
#include <string.h>
#include "H/cmd.h"
#include "H/ui.h"
#include "H/hub/auth.h"
#include "H/hub/hubcmd.h"

#ifndef ASH_VERSION
#define ASH_VERSION "dev"
#endif

static void print_usage(void) {
    printf("apm -- the Ash package manager\n\n");
    printf("working on a project:\n");
    printf("  apm new <name>        scaffold a module: ash.pkg + src/main/ash/<package>/<Class>.ash\n");
    printf("  apm init              create ash.pkg here\n");
    printf("  apm add <owner>/<repo>[@version] [--as <name>] [--dev]   add a package from the hub (github:<user>/<repo>[@ref] for any repo)\n");
    printf("  apm install           fetch [dependencies] and [dev-dependencies] into ./@ash-modules/\n");
    printf("  apm list              metadata, dependencies (and whether installed), libraries, scripts\n");
    printf("  apm check             verify the module follows the Ash module conventions\n");
    printf("  apm run <script>      run a [scripts] entry of ash.pkg (apm build / apm test = run build / run test)\n");
    printf("  apm license [<id>] [--author <name>] [--year <yyyy>]   list licenses / write LICENSE and set license: in ash.pkg\n\n");
    printf("the hub (hub.ash-project.org):\n");
    printf("  apm search <words...>   search packages          apm info <owner>/<repo>   show one package\n");
    printf("  apm auth login [--web | --with-token]   log in with the browser or a token\n");
    printf("  apm auth status | logout | token\n");
    printf("  apm link | sync | unlink [<owner>/<repo>]   list, refresh or remove your repository on the hub (CI: set APM_TOKEN)\n");
    printf("  apm packages            the packages you linked\n\n");
    printf("environment: APM_TOKEN (token, for CI), APM_HUB (another hub), APM_CONFIG_DIR, APM_GIT_BASE (git mirror)\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(); return 1; }

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0)) { print_usage(); return 0; }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) { printf("apm %s\n", ASH_VERSION); return 0; }
    const char *command = argv[1];
    int rest_argc = argc - 2;
    char **rest_argv = argv + 2;
    const char *project_dir = ".";

    if (strcmp(command, "new") == 0) return cmd_new(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "check") == 0) return cmd_check(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "init") == 0) return cmd_init(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "add") == 0) return cmd_add(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "install") == 0) return cmd_install(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "list") == 0) return cmd_list(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "license") == 0) return cmd_license(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "auth") == 0) return cmd_auth(rest_argc, rest_argv);
    if (strcmp(command, "link") == 0) return cmd_link(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "sync") == 0) return cmd_sync(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "unlink") == 0) return cmd_unlink(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "packages") == 0) return cmd_packages(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "search") == 0) return cmd_search(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "info") == 0) return cmd_info(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "run") == 0) return cmd_run(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "build") == 0 || strcmp(command, "test") == 0) {
        char *script[] = { (char *)command };
        return cmd_run(project_dir, 1, script);
    }

    ui_err("unknown command '%s'", command);
    print_usage();
    return 1;
}
