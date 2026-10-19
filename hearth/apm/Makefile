CC = gcc
.DEFAULT_GOAL := all
# make ASH_VERSION=<tag> bakes the release tag into --version (tools/release.sh does this)
ASH_VERSION ?= dev
CFLAGS = -DASH_VERSION='"$(ASH_VERSION)"' -Wall -Wextra -std=c11 -O2 -Isrc

APM_SRC = \
    src/C/main.c \
    src/C/manifest.c \
    src/C/fsutil.c \
    src/C/git_fetch.c \
    src/C/lock.c \
    src/C/install.c \
    src/C/cmd.c \
    src/C/ui.c \
    src/C/license/licenses.c \
    src/C/license/texts_permissive.c \
    src/C/license/texts_copyleft.c

# Objects and dependency files live in trees parallel to C/:
#   src/C/x.c -> src/O/x.o, src/D/x.d
obj_of = $(patsubst %.c,%.o,$(subst /C/,/O/,$(1)))
dep_of = $(patsubst %.c,%.d,$(subst /C/,/D/,$(1)))
APM_OBJ = $(foreach s,$(APM_SRC),$(call obj_of,$(s)))
APM_DEP = $(foreach s,$(APM_SRC),$(call dep_of,$(s)))

# -MMD -MP: see ashvm/Makefile's and kiln/Makefile's comment -- a shared
# header edit must rebuild every .o that includes it, not just the .c
# whose own timestamp changed.
define compile_rule
$(call obj_of,$(1)): $(1)
	@mkdir -p $$(dir $$@) $$(dir $(call dep_of,$(1)))
	$$(CC) $$(CFLAGS) -MMD -MP -MF $(call dep_of,$(1)) -MT $$@ -c $$< -o $$@
endef
$(foreach s,$(APM_SRC),$(eval $(call compile_rule,$(s))))

BIN = ../../bin/apm

all: $(BIN)

$(BIN): $(APM_OBJ)
	mkdir -p ../../bin
	$(CC) $(CFLAGS) -o $(BIN) $(APM_OBJ)

-include $(APM_DEP)

clean:
	find src -name '*.o' -delete
	find src -name '*.d' -delete
	rm -f $(BIN)

.PHONY: all clean
