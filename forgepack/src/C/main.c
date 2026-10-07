#include <stdio.h>
#include <string.h>
#include "H/cmd.h"
#include "H/ui.h"

static void print_usage(void) {
    printf("forgepack -- package manager for Ash (github: sources only, for now)\n\n");
    printf("usage:\n");
    printf("  forgepack init\n");
    printf("  forgepack add github:<user>/<repo>[@ref] [--as <name>]\n");
    printf("  forgepack install\n");
    printf("  forgepack list\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(); return 1; }

    const char *command = argv[1];
    int rest_argc = argc - 2;
    char **rest_argv = argv + 2;
    const char *project_dir = ".";

    if (strcmp(command, "init") == 0) return cmd_init(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "add") == 0) return cmd_add(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "install") == 0) return cmd_install(project_dir, rest_argc, rest_argv);
    if (strcmp(command, "list") == 0) return cmd_list(project_dir, rest_argc, rest_argv);

    ui_err("unknown command '%s'", command);
    print_usage();
    return 1;
}
