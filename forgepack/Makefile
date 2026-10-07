CC = gcc
.DEFAULT_GOAL := all
CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc

FORGEPACK_SRC = \
    src/C/main.c \
    src/C/manifest.c \
    src/C/fsutil.c \
    src/C/git_fetch.c \
    src/C/install.c \
    src/C/cmd.c \
    src/C/ui.c

# Objects and dependency files live in trees parallel to C/:
#   src/C/x.c -> src/O/x.o, src/D/x.d
obj_of = $(patsubst %.c,%.o,$(subst /C/,/O/,$(1)))
dep_of = $(patsubst %.c,%.d,$(subst /C/,/D/,$(1)))
FORGEPACK_OBJ = $(foreach s,$(FORGEPACK_SRC),$(call obj_of,$(s)))
FORGEPACK_DEP = $(foreach s,$(FORGEPACK_SRC),$(call dep_of,$(s)))

# -MMD -MP: see ashvm/Makefile's and kiln/Makefile's comment -- a shared
# header edit must rebuild every .o that includes it, not just the .c
# whose own timestamp changed.
define compile_rule
$(call obj_of,$(1)): $(1)
	@mkdir -p $$(dir $$@) $$(dir $(call dep_of,$(1)))
	$$(CC) $$(CFLAGS) -MMD -MP -MF $(call dep_of,$(1)) -MT $$@ -c $$< -o $$@
endef
$(foreach s,$(FORGEPACK_SRC),$(eval $(call compile_rule,$(s))))

BIN = ../bin/forgepack

all: $(BIN)

$(BIN): $(FORGEPACK_OBJ)
	mkdir -p ../bin
	$(CC) $(CFLAGS) -o $(BIN) $(FORGEPACK_OBJ)

-include $(FORGEPACK_DEP)

clean:
	find src -name '*.o' -delete
	find src -name '*.d' -delete
	rm -f $(BIN)

.PHONY: all clean
