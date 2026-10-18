# Mnemosyne: memory safety for Ash. A Rust static library with a C ABI (H/flow/flow.h) that both
# ashvm and kiln link, built to ../../bin/lib/mnemo.a. The crate is Rust/; cargo is the only build step.
# A missing Rust toolchain is a hard error (run tools/install.sh), never a silent skip.
CARGO ?= $(shell command -v cargo 2>/dev/null || echo $(HOME)/.cargo/bin/cargo)
LIB = ../../bin/lib/mnemo.a
CRATE = Rust

.DEFAULT_GOAL := all
all: $(LIB)

$(LIB): FORCE
	@test -x "$(CARGO)" || { echo "mnemo: cargo not found; run tools/install.sh" >&2; exit 1; }
	cd $(CRATE) && "$(CARGO)" build --release --quiet
	mkdir -p ../../bin/lib
	cp $(CRATE)/target/release/libmnemo.a $(LIB)

FORCE:

clean:
	rm -f $(LIB)
	rm -rf $(CRATE)/target

.PHONY: all clean FORCE
