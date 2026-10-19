#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser/H/core/parser.h"
#include "codegen/H/emit/emit.h"
#include "elf/H/elf_writer.h"
#include "elf/H/elf_dynamic.h"
#include "elf/H/elf_so_writer.h"
#include "pe/H/pe_writer.h"
#include "pe/H/pe_dll_writer.h"
#include "diagnostics/H/diagnostics.h"
#include "jdriver/H/jdriver.h"
#include "jlower/H/jlower.h"
#include "codegen/H/runtime/heap.h"
#include "app/H/target.h"

#ifndef ASH_VERSION
#define ASH_VERSION "dev"
#endif

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s <file.ash> -o <output> [--target=linux|windows] [--link=static|shared] [--wrap-int]\n", prog);
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
    if (argc == 2 && strcmp(argv[1], "--version") == 0) { printf("kiln %s\n", ASH_VERSION); return 0; }
    // --emit-runtime -o <path> [--target=...]: writes libkilnrt.so/.dll
    // (Plan B) and exits, no .ash input at all -- an entirely separate
    // mode from compiling a program.
    if (argc >= 4 && argc <= 5 && strcmp(argv[1], "--emit-runtime") == 0 && strcmp(argv[2], "-o") == 0) {
        if (argc == 5) kiln_set_target(parse_target(argv[4], argv[0]));
        // The runtime's own globals-address computation (codegen/C/emit/layout.c)
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

    if (argc < 4 || argc > 7) usage(argv[0]);
    if (strcmp(argv[2], "-o") != 0) usage(argv[0]);

    const char *input_path = argv[1];
    const char *output_path = argv[3];

    int wrap_int = 0;
    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "--wrap-int") == 0) wrap_int = 1;
        else if (strncmp(argv[i], "--target=", 9) == 0) kiln_set_target(parse_target(argv[i], argv[0]));
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

    // Only the static builds (Linux and Windows) can give memory back (heap.h); --link=shared IR carries no frees.
    if (kiln_get_link_mode() == KILN_LINK_STATIC) jl_drops = JL_DROP_STRINGS | JL_DROP_ARRAYS | JL_DROP_OBJECTS | JL_DROP_MAPS | JL_DROP_ELEMS | JL_NATIVE_NEWARR | (getenv("KILN_NO_REGIONS") ? 0 : JL_REGION);
    char *ir = jdriver_compile_file(input_path, wrap_int);
    if (!ir) return 1;
    if (strstr(ir, "mem_free(") || strstr(ir, "mem_cat(") || strstr(ir, "mem_free_arr(") || strstr(ir, "mem_free_obj(") || strstr(ir, "mem_free_map(") || strstr(ir, "mem_free_elems(") || strstr(ir, "mem_newarr1(")) {
        heap_enable_free_lists(strstr(ir, "mem_free_arr(") != NULL || strstr(ir, "mem_free_obj(") != NULL || strstr(ir, "mem_free_map(") != NULL || strstr(ir, "mem_newarr1(") != NULL, strstr(ir, "mem_region_enter(") != NULL);
    }
    diagnostics_set_source(ir, "<internal ir>");

    CodeBuf machine_code;
    compile_program(ir, &machine_code);

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
