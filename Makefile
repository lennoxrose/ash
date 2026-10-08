# Ash - Makefile
#	`make` makes all subprojects, such as ashvm, kiln, forgepack, etc.
#	`make clean` cleans all subprojects.
#	`make @<subproject>` cleans and builds only that subproject, e.g. `make @ashvm`

SUBDIRS := $(patsubst %/Makefile,%,$(wildcard */Makefile))

.PHONY: all clean $(SUBDIRS) $(addprefix clean-,$(SUBDIRS))

all: clean-bin $(SUBDIRS)

clean-bin:
	rm -rf bin
	mkdir -p bin

$(SUBDIRS):
	$(MAKE) -C $@ clean
	$(MAKE) -C $@

clean: $(addprefix clean-,$(SUBDIRS))

$(addprefix clean-,$(SUBDIRS)):
	$(MAKE) -C $(patsubst clean-%,%,$@) clean

@%:
	@if [ ! -f $*/Makefile ]; then \
		echo "no such subproject: $*" >&2; \
		exit 1; \
	fi
	$(MAKE) -C $* clean
	$(MAKE) -C $*
