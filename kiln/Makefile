CC = gcc
.DEFAULT_GOAL := all
CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc
LDFLAGS =

KILN_SRC = \
    src/app/C/main.c \
    src/app/C/target.c \
    src/lexer/C/lexer.c \
    src/lexer/C/lexer_import.c \
    src/parser/C/core/parser.c \
    src/parser/C/declarations/constants.c \
    src/parser/C/declarations/functions.c \
    src/parser/C/declarations/module_state.c \
    src/parser/C/declarations/vars.c \
    src/parser/C/imports/import_paths.c \
    src/parser/C/imports/imports.c \
    src/parser/C/statements/each_loop.c \
    src/parser/C/statements/loop_stack.c \
    src/parser/C/statements/parser_control.c \
    src/parser/C/statements/attempt_handle.c \
    src/codegen/C/builtins/builtins.c \
    src/codegen/C/builtins/exit_builtin.c \
    src/codegen/C/builtins/filter_builtin.c \
    src/codegen/C/builtins/map_builtin.c \
    src/codegen/C/builtins/type_builtin.c \
    src/codegen/C/builtins/math_builtins.c \
    src/codegen/C/builtins/num_builtin.c \
    src/codegen/C/builtins/reduce_builtin.c \
    src/codegen/C/builtins/sort_builtin.c \
    src/codegen/C/builtins/str_builtin.c \
    src/codegen/C/collections/array_mutate.c \
    src/codegen/C/collections/arrays.c \
    src/codegen/C/collections/keys_values.c \
    src/codegen/C/collections/maps.c \
    src/codegen/C/collections/maps_mutate.c \
    src/codegen/C/collections/matrix_mul.c \
    src/codegen/C/emit/bytes.c \
    src/codegen/C/emit/emit.c \
    src/codegen/C/emit/emit_mem.c \
    src/codegen/C/emit/emit_sse.c \
    src/codegen/C/emit/layout.c \
    src/codegen/C/expressions/expr.c \
    src/codegen/C/expressions/expr_bool.c \
    src/codegen/C/expressions/expr_call.c \
    src/codegen/C/functions/closures.c \
    src/codegen/C/functions/lambda.c \
    src/codegen/C/io/argv_builtin.c \
    src/codegen/C/io/argv_builtin_windows.c \
    src/codegen/C/io/file_path.c \
    src/codegen/C/io/fs_builtins.c \
    src/codegen/C/io/input_builtin.c \
    src/codegen/C/io/read_file_builtin.c \
    src/codegen/C/io/write_file_builtins.c \
    src/codegen/C/platform/platform_console.c \
    src/codegen/C/platform/platform_file.c \
    src/codegen/C/platform/linux_call.c \
    src/codegen/C/platform/win_call.c \
    src/codegen/C/platform/win_file_call.c \
    src/codegen/C/runtime/errors.c \
    src/codegen/C/runtime/heap.c \
    src/codegen/C/runtime/number_format.c \
    src/codegen/C/runtime/print_int.c \
    src/codegen/C/strings/affix_builtins.c \
    src/codegen/C/strings/case_builtins.c \
    src/codegen/C/strings/char_builtins.c \
    src/codegen/C/strings/indexof_builtin.c \
    src/codegen/C/strings/join_builtin.c \
    src/codegen/C/strings/replace_builtin.c \
    src/codegen/C/strings/split_builtin.c \
    src/codegen/C/strings/string_alloc.c \
    src/codegen/C/strings/strings.c \
    src/codegen/C/strings/substring_builtin.c \
    src/codegen/C/strings/trim_builtin.c \
    src/elf/C/elf_dynamic.c \
    src/elf/C/elf_dynamic_call.c \
    src/elf/C/elf_so_writer.c \
    src/elf/C/elf_writer.c \
    src/pe/C/pe_dll_writer.c \
    src/pe/C/pe_dynamic.c \
    src/pe/C/pe_writer.c \
    src/diagnostics/C/diagnostics.c

# Objects and dependency files live in trees parallel to each C tree:
#   src/<subsystem>/C/<feature>/x.c -> src/<subsystem>/O/<feature>/x.o
#                                      src/<subsystem>/D/<feature>/x.d
obj_of = $(patsubst %.c,%.o,$(subst /C/,/O/,$(1)))
dep_of = $(patsubst %.c,%.d,$(subst /C/,/D/,$(1)))
KILN_OBJ = $(foreach s,$(KILN_SRC),$(call obj_of,$(s)))
KILN_DEP = $(foreach s,$(KILN_SRC),$(call dep_of,$(s)))

# -MMD -MP: emit a per-object .d file listing the headers it includes, so
# editing a shared header (e.g. vars.h) correctly triggers a rebuild of
# every .o that includes it, not just files whose own .c changed. Without
# this, make's only dependency is "the .c is newer than the .o" -- a
# header-only edit silently produces a binary built from mismatched
# offsets/constants across files, with no error, until something crashes
# at runtime for reasons the source doesn't explain.
define compile_rule
$(call obj_of,$(1)): $(1)
	@mkdir -p $$(dir $$@) $$(dir $(call dep_of,$(1)))
	$$(CC) $$(CFLAGS) -MMD -MP -MF $(call dep_of,$(1)) -MT $$@ -c $$< -o $$@
endef
$(foreach s,$(KILN_SRC),$(eval $(call compile_rule,$(s))))

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

.PHONY: all clean rebuild install-runtime runtime-dll

rebuild: clean all