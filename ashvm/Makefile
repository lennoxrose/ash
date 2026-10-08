CC = gcc
.DEFAULT_GOAL := all
# -I../pyre/src, -fopenmp and the ../bin/extensions/pyre/libpyre.a/-ldl/-lvulkan/-lstdc++
# below: matrix_mul (src/builtins/C/matrix.c) links against Pyre's CPU +
# Vulkan backends, STATICALLY (the explicit .a path, not -lashgpu --
# bin/extensions/pyre/ also holds libpyre.so for kiln's --link=shared to find at
# runtime, and a bare -lashgpu would silently prefer that .so over the
# .a, turning ashvm from self-contained into "needs libpyre.so
# findable at runtime with no rpath to say where" -- found exactly that
# way when pyre/Makefile grew the .so target). ashvm already links
# against libc/libm normally, unlike kiln's hand-rolled, zero-dependency
# ELF/PE output, so this one extra build-time dependency doesn't break
# any existing promise (see ideas/assigned.md). -lstdc++ because
# libpyre.a's Vulkan backend is C++ (RAII) internally, even though
# ash_gpu.h itself is a plain C ABI -- plain gcc as the final linker
# needs that spelled out, g++ would do it automatically.
include ../pyre/cuda.mk

CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc -I../pyre/src -fopenmp
LDFLAGS = -lm ../bin/extensions/pyre/libpyre.a -fopenmp -ldl -lvulkan -lstdc++ $(CUDA_LIBS)

ASHVM_SRC = \
    src/app/C/main.c \
    src/lexer/C/lexer.c \
    src/lexer/C/lexer_import.c \
    src/value/C/hashmap.c \
    src/value/C/value.c \
    src/vm/C/api.c \
    src/vm/C/chunk.c \
    src/vm/C/dispatch.c \
    src/vm/C/errors.c \
    src/vm/C/state.c \
    src/compiler/C/calls.c \
    src/compiler/C/constants.c \
    src/compiler/C/expr.c \
    src/compiler/C/import_paths.c \
    src/compiler/C/loop_stack.c \
    src/compiler/C/state.c \
    src/compiler/C/stmt.c \
    src/compiler/C/stmt_control.c \
    src/compiler/C/stmt_import.c \
    src/compiler/C/module_state.c \
    src/compiler/C/prescan.c \
    src/builtins/C/builtins.c \
    src/builtins/C/collection.c \
    src/builtins/C/core.c \
    src/builtins/C/file.c \
    src/builtins/C/string.c \
    src/builtins/C/matrix.c \
    src/diagnostics/C/diagnostics.c

# Objects and dependency files live in trees parallel to each C tree:
#   src/<subsystem>/C/<feature>/x.c -> src/<subsystem>/O/<feature>/x.o
#                                      src/<subsystem>/D/<feature>/x.d
obj_of = $(patsubst %.c,%.o,$(subst /C/,/O/,$(1)))
dep_of = $(patsubst %.c,%.d,$(subst /C/,/D/,$(1)))
ASHVM_OBJ = $(foreach s,$(ASHVM_SRC),$(call obj_of,$(s)))
ASHVM_DEP = $(foreach s,$(ASHVM_SRC),$(call dep_of,$(s)))

# -MMD -MP: emit a per-object .d file listing the headers it includes, so
# editing a shared header (e.g. lexer.h, value.h, chunk.h) correctly
# triggers a rebuild of every .o that includes it, not just files whose
# own .c changed. Without this, make's only dependency is "the .c is newer
# than the .o" -- a header-only edit silently produces a binary built from
# mismatched token/opcode numbering across files, with no compile error,
# until something fails at runtime for reasons the source doesn't explain
# (this bit for real during the kiln-parity work this mirrors kiln's own
# Makefile comment for -- see kiln/Makefile).
define compile_rule
$(call obj_of,$(1)): $(1)
	@mkdir -p $$(dir $$@) $$(dir $(call dep_of,$(1)))
	$$(CC) $$(CFLAGS) -MMD -MP -MF $(call dep_of,$(1)) -MT $$@ -c $$< -o $$@
endef
$(foreach s,$(ASHVM_SRC),$(eval $(call compile_rule,$(s))))

BIN = ../bin/ashvm

all: $(BIN)

../bin/extensions/pyre/libpyre.a:
	$(MAKE) -C ../pyre lib

$(BIN): $(ASHVM_OBJ) ../bin/extensions/pyre/libpyre.a
	mkdir -p ../bin
	$(CC) $(CFLAGS) -o $(BIN) $(ASHVM_OBJ) $(LDFLAGS)

-include $(ASHVM_DEP)

clean:
	find src -name '*.o' -delete
	find src -name '*.d' -delete
	rm -f $(BIN)

.PHONY: all clean
