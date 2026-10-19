#include <stdio.h>
#include <string.h>
#include "H/cmd.h"
#include "H/ui.h"

#ifndef ASH_VERSION
#define ASH_VERSION "dev"
#endif

static void print_usage(void) {
    printf("apm -- the Ash package manager (github: sources only, for now)\n\n");
    printf("usage:\n");
    printf("  apm new <name>        scaffold a module: ash.pkg + src/main/ash/<package>/<Class>.ash\n");
    printf("  apm check             verify the module follows the Ash module conventions\n");
    printf("  apm init\n");
    printf("  apm add github:<user>/<repo>[@ref] [--as <name>] [--dev]\n");
    printf("  apm remove <name>     take a dependency out of ash.pkg and @ash-modules (and what only it needed)\n");
    printf("  apm install [--locked]   fetch the dependencies, at the commits ash.lock records (--locked: change nothing)\n");
    printf("  apm update [<name>]   look dependencies up again and move ash.lock to the commits found\n");
    printf("  apm list\n");
    printf("  apm license [<id>] [--author <name>] [--year <yyyy>]   list licenses / write LICENSE and set license: in ash.pkg\n");
    printf("  apm run <script>      run a [scripts] entry of ash.pkg (apm build / apm test = run build / run test)\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(); return 1; }

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
    if (strcmp(command, "update") == 0) return cmd_update(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "remove") == 0) return cmd_remove(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "list") == 0) return cmd_list(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "license") == 0) return cmd_license(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "run") == 0) return cmd_run(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "build") == 0 || strcmp(command, "test") == 0) {
        char *script[] = { (char *)command };
        return cmd_run(project_dir, 1, script);
    }

    ui_err("unknown command '%s'", command);
    print_usage();
    return 1;
}
