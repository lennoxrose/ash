CC = gcc
.DEFAULT_GOAL := all
CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc -I.. 

# Shared front end for Ash: lexer, parser, AST, type checker, lowering and the internal IR
# lexer, built as front.a and linked by both ashvm and kiln.
# There is no front-end executable: ashvm and kiln are the user-facing tools.
# Same layout rule as the other subprojects: src/<sub>/C/x.c -> src/<sub>/O/x.o.
FRONT_SRC = \
    src/ir/C/ir_lex.c \
    src/jcommon/C/jcommon.c \
    src/jcore/C/jcore.c \
    src/jcore/C/core_lang.c \
    src/jcore/C/core_function.c \
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
    src/jtypes/C/suggest.c \
    src/jown/C/own.c \
    src/jown/C/range.c \
    src/jown/C/region.c \
    src/jlower/C/lower_expr.c \
    src/jlower/C/lower_stmt.c \
    src/jlower/C/lower_class.c \
    src/jlower/C/lower_prelude.c \
    src/jlower/C/lower_prelude_long.c \
    src/jlower/C/lower_prelude_fmt.c \
    src/jlower/C/lower_prune.c \
    src/jdriver/C/jdriver.c \
    src/jfmt/C/jfmt.c \
    src/jexplain/C/jexplain.c \
    src/jexplain/C/explain_data.c \
    src/jrepl/C/jrepl.c

obj_of = $(patsubst %.c,%.o,$(subst /C/,/O/,$(1)))
dep_of = $(patsubst %.c,%.d,$(subst /C/,/D/,$(1)))
FRONT_OBJ = $(foreach s,$(FRONT_SRC),$(call obj_of,$(s)))
ALL_DEP = $(foreach s,$(FRONT_SRC),$(call dep_of,$(s)))

define compile_rule
$(call obj_of,$(1)): $(1)
	@mkdir -p $$(dir $$@) $$(dir $(call dep_of,$(1)))
	$$(CC) $$(CFLAGS) -MMD -MP -MF $(call dep_of,$(1)) -MT $$@ -c $$< -o $$@
endef
$(foreach s,$(FRONT_SRC),$(eval $(call compile_rule,$(s))))

LIB = ../../bin/lib/front.a

all: $(LIB)

$(LIB): $(FRONT_OBJ)
	mkdir -p ../../bin/lib
	ar rcs $@ $(FRONT_OBJ)

-include $(ALL_DEP)

clean:
	find src -name '*.o' -delete
	find src -name '*.d' -delete
	rm -f $(LIB)

.PHONY: all clean
