#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser/parser.h"
#include "codegen/emit.h"
#include "elf/elf_writer.h"
#include "elf/elf_dynamic.h"
#include "elf/elf_so_writer.h"
#include "pe/pe_writer.h"
#include "pe/pe_dll_writer.h"
#include "diagnostics/diagnostics.h"
#include "target.h"

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) { fprintf(stderr, "kiln: could not open file: %s\n", path); exit(1); }
    fseek(file, 0L, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *buffer = malloc((size_t)size + 1);
    fread(buffer, 1, (size_t)size, file);
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s <file.ash> -o <output> [--target=linux|windows] [--link=static|shared]\n", prog);
    fprintf(stderr, "       %s --emit-runtime -o <libkilnrt.so|libkilnrt.dll> [--target=linux|windows]\n", prog);
    exit(1);
}

static KilnTarget parse_target(const char *arg, const char *prog) {
    if (strcmp(arg, "--target=linux") == 0) return KILN_TARGET_LINUX;
    if (strcmp(arg, "--target=windows") == 0) return KILN_TARGET_WINDOWS;
    fprintf(stderr, "kiln: unrecognized target '%s' (expected linux or windows)\n", arg);
    usage(prog);
    return KILN_TARGET_LINUX; // unreachable -- usage() exits
}

static KilnLinkMode parse_link(const char *arg, const char *prog) {
    if (strcmp(arg, "--link=static") == 0) return KILN_LINK_STATIC;
    if (strcmp(arg, "--link=shared") == 0) return KILN_LINK_SHARED;
    fprintf(stderr, "kiln: unrecognized link mode '%s' (expected static or shared)\n", arg);
    usage(prog);
    return KILN_LINK_STATIC; // unreachable
}

int main(int argc, char *argv[]) {
    // --emit-runtime -o <path> [--target=...]: writes libkilnrt.so/.dll
    // (Plan B) and exits, no .ash input at all -- an entirely separate
    // mode from compiling a program.
    if (argc >= 4 && argc <= 5 && strcmp(argv[1], "--emit-runtime") == 0 && strcmp(argv[2], "-o") == 0) {
        if (argc == 5) kiln_set_target(parse_target(argv[4], argv[0]));
        // The runtime's own globals-address computation (codegen/layout.c)
        // needs to resolve as if for a --link=shared executable, since
        // that's whose globals this code reads/writes at runtime --
        // elf_write_shared_runtime/pe_write_shared_runtime additionally
        // set kiln_set_compiling_so to keep the runtime's OWN internal
        // calls (e.g. string_alloc calling heap_alloc) local instead of
        // calls back into itself.
        kiln_set_link_mode(KILN_LINK_SHARED);
        return kiln_get_target() == KILN_TARGET_WINDOWS
            ? pe_write_shared_runtime(argv[3])
            : elf_write_shared_runtime(argv[3]);
    }

    if (argc < 4 || argc > 6) usage(argv[0]);
    if (strcmp(argv[2], "-o") != 0) usage(argv[0]);

    const char *input_path = argv[1];
    const char *output_path = argv[3];

    for (int i = 4; i < argc; i++) {
        if (strncmp(argv[i], "--target=", 9) == 0) kiln_set_target(parse_target(argv[i], argv[0]));
        else if (strncmp(argv[i], "--link=", 7) == 0) kiln_set_link_mode(parse_link(argv[i], argv[0]));
        else usage(argv[0]);
    }

    // A Windows binary without a .exe suffix won't be recognized as
    // executable by the shell/loader -- match what every other compiler
    // targeting Windows does and append it when the caller didn't already.
    char *owned_output_path = NULL;
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        size_t len = strlen(output_path);
        int has_exe = len >= 4 && strcmp(output_path + len - 4, ".exe") == 0;
        if (!has_exe) {
            owned_output_path = malloc(len + 5);
            memcpy(owned_output_path, output_path, len);
            memcpy(owned_output_path + len, ".exe", 5);
            output_path = owned_output_path;
        }
    }

    char *source = read_file(input_path);
    diagnostics_set_source(source, input_path);

    CodeBuf machine_code;
    compile_program(source, input_path, &machine_code);
    free(source);

    int status;
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        status = kiln_get_link_mode() == KILN_LINK_SHARED
            ? pe_write_dynamic_executable(output_path, &machine_code)
            : pe_write_executable(output_path, &machine_code);
    } else {
        status = kiln_get_link_mode() == KILN_LINK_SHARED
            ? elf_write_dynamic_executable(output_path, &machine_code)
            : elf_write_executable(output_path, &machine_code);
    }
    free(machine_code.code);
    free(owned_output_path);
    return status;
}
