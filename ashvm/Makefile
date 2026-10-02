CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc
LDFLAGS = -lm

ASHVM_SRC = src/main.c src/lexer/lexer.c src/lexer/lexer_import.c src/value/value.c src/value/hashmap.c src/vm/chunk.c \
            src/compiler/state.c src/compiler/expr.c src/compiler/calls.c src/compiler/stmt.c src/compiler/stmt_control.c \
            src/compiler/import_paths.c src/compiler/stmt_import.c src/compiler/constants.c \
            src/builtins/builtins.c src/builtins/core.c src/builtins/collection.c src/builtins/string.c src/builtins/file.c \
            src/vm/state.c src/vm/errors.c src/vm/dispatch.c src/vm/api.c \
            src/diagnostics/diagnostics.c
ASHVM_OBJ = $(ASHVM_SRC:.c=.o)

BIN = ../bin/ashvm

all: $(BIN)

$(BIN): $(ASHVM_OBJ)
	mkdir -p ../bin
	$(CC) $(CFLAGS) -o $(BIN) $(ASHVM_OBJ) $(LDFLAGS)

clean:
	find src -name '*.o' -delete
	rm -f $(BIN)

.PHONY: all clean
