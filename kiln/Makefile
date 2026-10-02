CC = gcc
# -MMD -MP: emit a per-object .d file listing the headers it includes, so
# editing a shared header (e.g. vars.h) correctly triggers a rebuild of
# every .o that includes it, not just files whose own .c changed. Without
# this, make's only dependency is "the .c is newer than the .o" -- a
# header-only edit silently produces a binary built from mismatched
# offsets/constants across files, with no error, until something crashes
# at runtime for reasons the source doesn't explain.
CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc -MMD -MP
LDFLAGS =

KILN_SRC = src/main.c src/target.c src/lexer/lexer.c src/lexer/lexer_import.c \
           src/parser/parser.c src/parser/parser_control.c src/parser/vars.c src/parser/functions.c src/parser/try_catch.c src/parser/for_loop.c src/parser/loop_stack.c \
           src/parser/import_paths.c src/parser/imports.c src/parser/constants.c \
           src/codegen/emit.c src/codegen/emit_mem.c src/codegen/emit_sse.c src/codegen/heap.c src/codegen/bytes.c src/codegen/layout.c \
           src/codegen/strings.c src/codegen/errors.c src/codegen/arrays.c src/codegen/maps.c src/codegen/maps_mutate.c src/codegen/builtins.c \
           src/codegen/math_builtins.c src/codegen/str_builtin.c src/codegen/num_builtin.c \
           src/codegen/map_builtin.c src/codegen/filter_builtin.c src/codegen/reduce_builtin.c src/codegen/keys_values.c \
           src/codegen/string_alloc.c src/codegen/case_builtins.c src/codegen/trim_builtin.c \
           src/codegen/substring_builtin.c src/codegen/indexof_builtin.c \
           src/codegen/join_builtin.c src/codegen/split_builtin.c src/codegen/replace_builtin.c \
           src/codegen/file_path.c src/codegen/read_file_builtin.c src/codegen/write_file_builtins.c \
           src/codegen/input_builtin.c src/codegen/argv_builtin.c src/codegen/argv_builtin_windows.c \
           src/codegen/expr.c src/codegen/expr_call.c src/codegen/expr_bool.c src/codegen/print_int.c \
           src/codegen/closures.c src/codegen/lambda.c src/codegen/win_call.c src/codegen/win_file_call.c \
           src/codegen/platform_console.c src/codegen/platform_file.c \
           src/elf/elf_writer.c src/elf/elf_dynamic.c src/elf/elf_dynamic_call.c src/elf/elf_so_writer.c \
           src/pe/pe_writer.c src/pe/pe_dynamic.c src/pe/pe_dll_writer.c \
           src/diagnostics/diagnostics.c
KILN_OBJ = $(KILN_SRC:.c=.o)
KILN_DEP = $(KILN_SRC:.c=.d)

BIN = ../bin/kiln

all: $(BIN)

$(BIN): $(KILN_OBJ)
	mkdir -p ../bin
	$(CC) $(CFLAGS) -o $(BIN) $(KILN_OBJ) $(LDFLAGS)

-include $(KILN_DEP)

clean:
	find src -name '*.o' -delete
	find src -name '*.d' -delete
	rm -f $(BIN)

# Plan B: builds libkilnrt.so and installs it at the fixed path every
# --link=shared kiln-compiled program's DT_RPATH points at (see
# src/elf/elf_dynamic.h's KILN_RPATH) -- needs root to write there, so
# this is a separate, explicit, opt-in step, never run by `make all`.
KILNRT = ../bin/libkilnrt.so
KILN_RPATH_DIR = /usr/local/lib/kiln

$(KILNRT): $(BIN)
	$(BIN) --emit-runtime -o $(KILNRT)

install-runtime: $(KILNRT)
	install -d $(KILN_RPATH_DIR)
	install -m 0755 $(KILNRT) $(KILN_RPATH_DIR)/libkilnrt.so

# Windows equivalent: no install step needed -- the standard DLL search
# order checks the launching .exe's own directory first, so
# libkilnrt.dll just needs to sit next to whatever --link=shared
# --target=windows binary is using it (or on PATH).
KILNRT_DLL = ../bin/libkilnrt.dll

$(KILNRT_DLL): $(BIN)
	$(BIN) --emit-runtime -o $(KILNRT_DLL) --target=windows

runtime-dll: $(KILNRT_DLL)

.PHONY: all clean install-runtime runtime-dll
