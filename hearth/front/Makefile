CC = gcc
.DEFAULT_GOAL := all
CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc -DASH_STDLIB_DIR=\"$(abspath stdlib)\"

# Shared front end for new (Java-like) Ash: lexer, parser, AST, type checker and
# lowering, linked by both ashvm and kiln (see MIGRATION.md, decision D12).
# Same layout rule as the other subprojects: src/<sub>/C/x.c -> src/<sub>/O/x.o.
FRONT_SRC = \
    src/jcommon/C/jcommon.c \
    src/jlex/C/jlex.c \
    src/jparse/C/jparse_decl.c \
    src/jparse/C/jparse_expr.c \
    src/jparse/C/jparse_stmt.c \
    src/jparse/C/jparse_old.c \
    src/jtypes/C/types.c \
    src/jtypes/C/classes.c \
    src/jtypes/C/check_expr.c \
    src/jtypes/C/check_stmt.c \
    src/jtypes/C/check_class.c \
    src/jlower/C/lower_expr.c \
    src/jlower/C/lower_stmt.c \
    src/jlower/C/lower_class.c \
    src/jlower/C/lower_prelude.c \
    src/jlower/C/lower_prelude_long.c \
    src/jlower/C/lower_prune.c

obj_of = $(patsubst %.c,%.o,$(subst /C/,/O/,$(1)))
dep_of = $(patsubst %.c,%.d,$(subst /C/,/D/,$(1)))
FRONT_OBJ = $(foreach s,$(FRONT_SRC),$(call obj_of,$(s)))
APP_SRC = src/app/C/main.c
APP_OBJ = $(foreach s,$(APP_SRC),$(call obj_of,$(s)))
ALL_DEP = $(foreach s,$(FRONT_SRC) $(APP_SRC),$(call dep_of,$(s)))

define compile_rule
$(call obj_of,$(1)): $(1)
	@mkdir -p $$(dir $$@) $$(dir $(call dep_of,$(1)))
	$$(CC) $$(CFLAGS) -MMD -MP -MF $(call dep_of,$(1)) -MT $$@ -c $$< -o $$@
endef
$(foreach s,$(FRONT_SRC) $(APP_SRC),$(eval $(call compile_rule,$(s))))

LIB = ../../bin/libashfront.a
TOOL = ../../bin/ashfront

all: $(LIB) $(TOOL)

lib: $(LIB)

$(LIB): $(FRONT_OBJ)
	mkdir -p ../../bin
	ar rcs $@ $(FRONT_OBJ)

$(TOOL): $(APP_OBJ) $(LIB)
	mkdir -p ../../bin
	$(CC) $(CFLAGS) -o $@ $(APP_OBJ) $(LIB)

-include $(ALL_DEP)

clean:
	find src -name '*.o' -delete
	find src -name '*.d' -delete
	rm -f $(LIB) $(TOOL)

.PHONY: all lib clean
