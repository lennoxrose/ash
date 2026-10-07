CC = gcc
.DEFAULT_GOAL := all
CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc
LDFLAGS = -lm

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
    src/compiler/C/state.c \
    src/compiler/C/stmt.c \
    src/compiler/C/stmt_control.c \
    src/compiler/C/stmt_import.c \
    src/builtins/C/builtins.c \
    src/builtins/C/collection.c \
    src/builtins/C/core.c \
    src/builtins/C/file.c \
    src/builtins/C/string.c \
    src/diagnostics/C/diagnostics.c

# Objects and dependency files live in trees parallel to each C tree:
#   src/<subsystem>/C/<feature>/x.c -> src/<subsystem>/O/<feature>/x.o
obj_of = $(patsubst %.c,%.o,$(subst /C/,/O/,$(1)))
ASHVM_OBJ = $(foreach s,$(ASHVM_SRC),$(call obj_of,$(s)))

define compile_rule
$(call obj_of,$(1)): $(1)
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) -c $$< -o $$@
endef
$(foreach s,$(ASHVM_SRC),$(eval $(call compile_rule,$(s))))

BIN = ../bin/ashvm

all: $(BIN)

$(BIN): $(ASHVM_OBJ)
	mkdir -p ../bin
	$(CC) $(CFLAGS) -o $(BIN) $(ASHVM_OBJ) $(LDFLAGS)

clean:
	find src -name '*.o' -delete
	rm -f $(BIN)

.PHONY: all clean
