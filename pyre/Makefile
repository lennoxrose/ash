# Pyre (ash-gpu): same C/H/D/O mirrored-tree convention as kiln/ashvm
# (AGENTS.md section 1), with feature folders nested inside each tree
# (section 1's "large ones add feature folders identically in all
# trees" shape, same as kiln/src/codegen/{C,H,D,O}/expressions/) instead
# of ideas/gpu_acceleration.md's standalone include/runtime/backends/
# layout -- this is one more subsystem inside the SAME multi-project
# repo, so it follows that repo's rules, just keeping that doc's
# meaningful feature names (runtime, backends/cpu, backends/vulkan) as
# the folders. .cpp sources (the Vulkan backend -- RAII, see that doc's
# own "Why C++ for the Runtime") live in the same C/ tree as the .c
# sources; AGENTS.md's "one file TYPE per tree" rule is about not mixing
# sources with headers/objects/deps, not about .c vs .cpp specifically.
#
# Phase 1/2 status (see pyre/todo/todo.md, ideas/assigned.md): CPU
# backend for both benchmark kernels, Vulkan backend for matrix_multiply
# only (written and compiled against the real SDK via
# pyre/tools/install_deps.sh, never run end-to-end -- this sandbox has
# no GPU at all).

include cuda.mk

CC = gcc
CXX = g++
.DEFAULT_GOAL := all
CFLAGS = -Wall -Wextra -std=c11 -O3 -march=native -fopenmp -Isrc -fPIC -pthread $(CUDA_DEFINE)
CXXFLAGS = -Wall -Wextra -std=c++17 -O3 -Isrc -fPIC -pthread
LDFLAGS = -fopenmp -lm -ldl -lvulkan $(CUDA_LIBS)
NVCCFLAGS = -std=c++17 -O3 $(CUDA_ARCH) -Isrc -Xcompiler -fPIC -Xcompiler -Wall

# Built libraries land in the repo's top-level bin/, next to ashvm and kiln,
# under extensions/ (Pyre is an optional add-on, not a core tool). ashvm links
# the .a from here; kiln's --link=shared binaries find the .so here via an
# rpath entry (see kiln/Makefile).
EXT_DIR = ../bin/extensions/pyre

PYRE_C_SRC = \
    src/C/runtime/device.c \
    src/C/runtime/targets.c \
    src/C/runtime/cost_model.c \
    src/C/runtime/manager.c \
    src/C/backends/cpu/matrix_multiply.c \
    src/C/backends/cpu/monte_carlo.c \
    src/C/backends/cpu/chaos.c
PYRE_CPP_SRC = \
    src/C/backends/vulkan/context.cpp \
    src/C/backends/vulkan/matrix_multiply.cpp \
    src/C/backends/vulkan/chaos.cpp
# NVIDIA-only CUDA backend (src/C/backends/cuda/*.cu), built only when
# cuda.mk found nvcc; without it Pyre is Vulkan + CPU exactly as before.
PYRE_CU_SRC = \
    src/C/backends/cuda/context.cu \
    src/C/backends/cuda/matrix_multiply.cu \
    src/C/backends/cuda/chaos.cu \
    src/C/backends/cuda/monte_carlo.cu
ifeq ($(NVCC),)
PYRE_CU_SRC =
endif

# Objects and dependency files live in trees parallel to C/:
#   src/C/<feature>/x.c   -> src/O/<feature>/x.o   src/D/<feature>/x.d
#   src/C/<feature>/x.cpp -> src/O/<feature>/x.o   src/D/<feature>/x.d
obj_of = $(subst /C/,/O/,$(basename $(1)).o)
dep_of = $(subst /C/,/D/,$(basename $(1)).d)
PYRE_OBJ = $(foreach s,$(PYRE_C_SRC) $(PYRE_CPP_SRC) $(PYRE_CU_SRC),$(call obj_of,$(s)))
PYRE_DEP = $(foreach s,$(PYRE_C_SRC) $(PYRE_CPP_SRC) $(PYRE_CU_SRC),$(call dep_of,$(s)))

# The Vulkan backend's one kernel so far needs its shader compiled to
# SPIR-V and embedded as a C array (glslc -mfmt=c) rather than loaded
# from disk at run time -- no installed-data-directory story exists yet,
# and a compile-time-embedded array needs none. Generated, not tracked
# (same as any other object code); src/shaders/matrix_multiply.comp is
# the tracked source.
src/H/backends/vulkan/matrix_multiply_spv.h: src/shaders/matrix_multiply.comp
	@mkdir -p $(dir $@)
	glslc -fshader-stage=compute --target-env=vulkan1.2 -O -mfmt=c -o $@.body $<
	{ echo 'static const uint32_t matrix_multiply_spv[] ='; cat $@.body; echo ';'; } > $@
	rm -f $@.body

src/O/backends/vulkan/matrix_multiply.o: src/H/backends/vulkan/matrix_multiply_spv.h

src/H/backends/vulkan/chaos_spv.h: src/shaders/chaos.comp
	@mkdir -p $(dir $@)
	glslc -fshader-stage=compute --target-env=vulkan1.2 -O -mfmt=c -o $@.body $<
	{ echo 'static const uint32_t chaos_spv[] ='; cat $@.body; echo ';'; } > $@
	rm -f $@.body

src/O/backends/vulkan/chaos.o: src/H/backends/vulkan/chaos_spv.h

# -MMD -MP: a header-only edit (src/H/runtime/ash_gpu.h) correctly
# rebuilds every .o that includes it, not just whichever source's own
# timestamp changed (see kiln/Makefile's identical comment for the
# incident this guards against).
define c_compile_rule
$(call obj_of,$(1)): $(1)
	@mkdir -p $$(dir $$@) $$(dir $(call dep_of,$(1)))
	$$(CC) $$(CFLAGS) -MMD -MP -MF $(call dep_of,$(1)) -MT $$@ -c $$< -o $$@
endef
$(foreach s,$(PYRE_C_SRC),$(eval $(call c_compile_rule,$(s))))

define cpp_compile_rule
$(call obj_of,$(1)): $(1)
	@mkdir -p $$(dir $$@) $$(dir $(call dep_of,$(1)))
	$$(CXX) $$(CXXFLAGS) -MMD -MP -MF $(call dep_of,$(1)) -MT $$@ -c $$< -o $$@
endef
$(foreach s,$(PYRE_CPP_SRC),$(eval $(call cpp_compile_rule,$(s))))

define cu_compile_rule
$(call obj_of,$(1)): $(1)
	@mkdir -p $$(dir $$@) $$(dir $(call dep_of,$(1)))
	$$(NVCC) $$(NVCCFLAGS) -MMD -MF $(call dep_of,$(1)) -MT $$@ -c $$< -o $$@
endef
$(foreach s,$(PYRE_CU_SRC),$(eval $(call cu_compile_rule,$(s))))

-include $(PYRE_DEP)

.PHONY: all clean test benchmark lib install tool web

all: test benchmark lib tool web

# Consumed by ashvm's Makefile (../bin/extensions/pyre/libpyre.a) -- see
# ashvm/src/builtins/C/matrix.c and ideas/assigned.md for why ashvm links
# against this directly while kiln, whose own output is a hand-rolled,
# zero-dependency ELF/PE binary, does not.
$(EXT_DIR)/libpyre.a: $(PYRE_OBJ)
	mkdir -p $(EXT_DIR)
	ar rcs $@ $(PYRE_OBJ)

# Consumed by kiln's --link=shared mode (elf/H/elf_dynamic.h's
# KILN_ASHGPU_SONAME, same rpath as libkilnrt.so -- see ideas/assigned.md):
# a real DT_NEEDED on a shared object built by the normal system
# toolchain, not another hand-rolled kiln-side writer like libkilnrt.so.
$(EXT_DIR)/libpyre.so: $(PYRE_OBJ)
	mkdir -p $(EXT_DIR)
	$(CXX) -shared -o $@ $(PYRE_OBJ) $(LDFLAGS)

lib: $(EXT_DIR)/libpyre.a $(EXT_DIR)/libpyre.so

# `pyre info|calibrate|plan ...`: inspect the manager (tools/pyre.c).
tools/%.o: tools/%.c tools/dashboard.h tools/json_out.h tools/train.h
	$(CC) $(CFLAGS) -D_GNU_SOURCE -c $< -o $@

$(EXT_DIR)/pyre: tools/pyre.o tools/dashboard.o tools/train.o $(PYRE_OBJ)
	mkdir -p $(EXT_DIR)
	$(CXX) $(CXXFLAGS) -fopenmp $^ -o $@ $(LDFLAGS)

tool: $(EXT_DIR)/pyre

# The dashboard UI (`pyre dashboard`): React + TypeScript in src/web, built
# with pnpm to <bin>/extensions/pyre/web, which the pyre executable serves
# fully offline. Node and pnpm are needed at BUILD time only (and network the
# first time, to fetch packages); without them Pyre still builds and only the
# dashboard UI is skipped, with a warning.
WEB_SRC = src/web
WEB_OUT = $(EXT_DIR)/web
WEB_DEPS = $(shell find $(WEB_SRC)/src -type f) $(addprefix $(WEB_SRC)/,index.html package.json pnpm-lock.yaml vite.config.ts tsconfig.json)

$(WEB_OUT)/index.html: $(WEB_DEPS)
	@if command -v pnpm >/dev/null 2>&1; then \
	    (cd $(WEB_SRC) && pnpm install --frozen-lockfile && pnpm build) \
	    || echo "WARNING: dashboard UI build failed (offline first build? run 'pnpm install' in pyre/$(WEB_SRC) once with network); 'pyre dashboard' will say it is not built"; \
	else \
	    echo "WARNING: pnpm not found, skipping the dashboard UI (install Node + pnpm, then 'make web')"; \
	fi

web: $(WEB_OUT)/index.html

# Installs libpyre.so at kiln's own rpath (KILN_RPATH, elf/H/elf_dynamic.h)
# so a kiln --link=shared executable that imports an ash_gpu_* symbol can
# actually find it at load time -- same spot `kiln --emit-runtime` installs
# libkilnrt.so to by hand today (there's no kiln "make install" yet either).
install: $(EXT_DIR)/libpyre.so
	mkdir -p /usr/local/lib/kiln
	cp $(EXT_DIR)/libpyre.so /usr/local/lib/kiln/libpyre.so

# Each test/benchmark main is plain C -- compiled with $(CC) into its own
# .o (g++ would compile a .c file as C++ instead, which is stricter than
# this project wants these particular files to care about, e.g. no
# implicit void* -> T* from malloc). $(CXX) only does the final LINK, so
# it can pull in the Vulkan backend's C++ objects (PYRE_OBJ) without an
# explicit -lstdc++.
%_test.o: %_test.c
	$(CC) $(CFLAGS) -c $< -o $@

benchmarks/%/run_benchmark.o: benchmarks/%/run_benchmark.c
	$(CC) $(CFLAGS) -c $< -o $@

tests/matrix_multiply_test: tests/matrix_multiply_test.o $(PYRE_OBJ)
	$(CXX) $(CXXFLAGS) -fopenmp $^ -o $@ $(LDFLAGS)

tests/monte_carlo_test: tests/monte_carlo_test.o $(PYRE_OBJ)
	$(CXX) $(CXXFLAGS) -fopenmp $^ -o $@ $(LDFLAGS)

tests/cuda_test: tests/cuda_test.o $(PYRE_OBJ)
	$(CXX) $(CXXFLAGS) -fopenmp $^ -o $@ $(LDFLAGS)

test: tests/matrix_multiply_test tests/monte_carlo_test tests/cuda_test
	tests/matrix_multiply_test
	tests/monte_carlo_test
	tests/cuda_test

benchmarks/matrix/run_benchmark: benchmarks/matrix/run_benchmark.o $(PYRE_OBJ)
	$(CXX) $(CXXFLAGS) -fopenmp $^ -o $@ $(LDFLAGS)

benchmarks/monte_carlo_no_pi/run_benchmark: benchmarks/monte_carlo_no_pi/run_benchmark.o $(PYRE_OBJ)
	$(CXX) $(CXXFLAGS) -fopenmp $^ -o $@ $(LDFLAGS)

benchmarks/chaos/run_benchmark: benchmarks/chaos/run_benchmark.o $(PYRE_OBJ)
	$(CXX) $(CXXFLAGS) -fopenmp $^ -o $@ $(LDFLAGS)

benchmark: benchmarks/matrix/run_benchmark benchmarks/monte_carlo_no_pi/run_benchmark benchmarks/chaos/run_benchmark
	benchmarks/matrix/run_benchmark
	benchmarks/monte_carlo_no_pi/run_benchmark
	benchmarks/chaos/run_benchmark

clean:
	find src -name '*.o' -delete
	find src -name '*.d' -delete
	rm -f src/H/backends/vulkan/matrix_multiply_spv.h src/H/backends/vulkan/chaos_spv.h
	rm -f tests/*.o benchmarks/*/*.o tools/*.o $(EXT_DIR)/pyre
	rm -rf $(WEB_OUT)
	rm -f tests/matrix_multiply_test tests/monte_carlo_test tests/cuda_test
	rm -f benchmarks/matrix/run_benchmark benchmarks/monte_carlo_no_pi/run_benchmark benchmarks/chaos/run_benchmark
	rm -f $(EXT_DIR)/libpyre.a $(EXT_DIR)/libpyre.so
