# Assigned: when ashvm and kiln use Pyre

This file didn't exist when the Pyre work started (checked exhaustively --
nowhere in the repo). It's written now, derived from ideas/gpu_acceleration.md's
own Development Phases, Dispatch and Design Principles sections, which is
the actual source of truth per that doc's priority over todo.md. Correct
this file directly if any rule below doesn't match intent.

## Phase order (ideas/gpu_acceleration.md's own table) -- follow this order, not todo.md's grouping

1. **Standalone runtime** -- detection, one backend, alloc/dispatch, CPU
   fallback, benchmarks. *Don't touch Ash yet.* **Done, with one real
   caveat**: `pyre/`'s dispatcher (`src/C/runtime/device.c`) tries a real
   Vulkan device via `ash_gpu_vulkan_init()`
   (`src/C/backends/vulkan/context.cpp`) before falling back to the CPU
   backend, and `ash_gpu_matrix_multiply` has a working Vulkan compute
   shader (`src/shaders/matrix_multiply.comp`) behind it -- but this
   sandbox has no GPU and no ICD at all (`vulkaninfo`: "Found no
   drivers!"), so every piece of that path compiles against the real
   Vulkan 1.2 SDK (`pyre/tools/install_deps.sh`) but has **never actually
   run**. Both of the user's benchmark challenges are beaten by 85%+
   margins on the CPU backend alone (see `pyre/todo/todo.md`) -- that
   part IS verified, repeatedly, on real hardware (this machine's own
   CPU). Whoever first runs this on the RX 9070 XT should expect to find
   and fix real bugs in the Vulkan path; nothing about it has been
   execution-tested, only reviewed and compiled.
2. **Primitive library** -- fast, benchmark-driven building blocks.
   **Done for two kernels**: `ash_gpu_matrix_multiply`, `ash_gpu_monte_carlo_risk`
   (`pyre/src/C/backends/cpu/`). More kernels land as more benchmarks or
   real call sites need them -- not speculatively ahead of demand.
3. **Ash bindings** -- "small API ... **no compiler changes**." **Done
   for ashvm** (see below): a plain builtin function that calls the C
   ABI, no new syntax, no parser/codegen changes to existing language
   constructs.
4. **Kiln integration** -- "Kiln emits calls to ash-gpu for suitable
   workloads," *only after the runtime is solid*. **Done**: kiln's
   generated code really does call into `libpyre.so` through a GOT
   slot now -- see "kiln" below for how. Found and fixed one real,
   previously-invisible bug this way (kiln's own `exit`-syscall choice),
   which is exactly the value of actually running this instead of
   stopping at "compiles."
5. **Automatic acceleration** -- plain loops become GPU workloads with
   no API call at all. **Not started**, needs escape/aliasing analysis
   neither engine has (goals.md item 1 territory). Don't attempt this
   before 1-4 are real and measured.

## ashvm: done (Phase 3)

`matrix_mul(a, b)` (`ashvm/src/builtins/C/matrix.c`) calls
`ash_gpu_matrix_multiply` directly. This is "ok" unconditionally at the
*build* level: ashvm already links against libc/libm like any normal C
program, so one more build-time dependency (Pyre's CPU backend needs
OpenMP/libgomp, `runtime/device.c` needs libdl for its detection probe)
doesn't break any promise ashvm makes. ashvm's own Makefile links
`../pyre/lib/libpyre.a` unconditionally -- there's no "Pyre not
installed" fallback path today; if that matters later (packaging ashvm
without Pyre), add a `HAVE_PYRE` build switch then, not speculatively now.

**Per-call dispatch rule** (derives the ideas doc's "small workload ->
CPU, don't accelerate every operation" principle into an actual number):
`ASH_GPU_MIN_N = 64`. Below it, `matrix_mul` computes with a plain local
triple loop instead of calling into Pyre -- OpenMP's thread-launch
overhead would dominate a handful of multiply-adds. At or above it,
Pyre's CPU backend (ikj loop order + OpenMP) runs unconditionally. 64 is
a first guess, not a measured threshold; revisit with a benchmark across
sizes if it ever matters.

## kiln: done, under --link=shared only

kiln's compiled *output* is a hand-rolled, zero-dependency ELF/PE binary
by default (AGENTS.md: "Ash is a from-scratch programming language in
C, no external dependencies" -- this is kiln's whole reason to exist,
not incidental). Pyre's CPU backend needs OpenMP (libgomp) and `dlopen`
(libdl). Linking *every* kiln-compiled program against those
unconditionally would quietly break that promise for every Ash program,
not just the ones that use `matrix_mul` -- not "ok" by default. So
`matrix_mul(a, b)` (`codegen/C/collections/matrix_mul.c`) is available only
under `--link=shared`; under the default `--link=static` it's a compile
error naming exactly why, not a silent fallback.

How: `--link=shared` already had a real, working example of exactly
this shape -- it emits a dynamically-linked ELF with a `PT_INTERP`, so
the real system `ld.so` resolves calls into a second binary
(`libkilnrt.so`) at load time (`elf/C/elf_dynamic.c`, `elf/C/elf_so_writer.c`).
Extended that same machinery to also declare `libpyre.so` as a SECOND
needed library: one more `DT_NEEDED`, and `libpyre.so`'s one export
(`ash_gpu_matrix_multiply`) folded into the SAME combined hash/dynsym/GOT/rela
table as kiln's own runtime imports (`KILN_DYN_TOTAL_IMPORTS`,
`elf/H/elf_dynamic.h`) rather than a second parallel set of tables --
`ld.so` resolves a symbol name against every needed library regardless
of which table region declared it, so one combined table is correct and
the much smaller change. `codegen/C/platform/linux_call.c` added the
one piece kiln never needed before (calling a REAL, normally-compiled
C/C++ function instead of one of kiln's own alignment-agnostic
hand-rolled routines): a SysV ABI call wrapper that dynamically
16-byte-aligns RSP first, same technique `win_call.c` already uses for
Windows, reusing its scratch slot (the two targets are never both live
in the same compiled program). `matrix_mul` itself flattens its nested
array-of-arrays arguments into temporary flat buffers, calls through
the GOT slot, and unflattens the result -- the bridge `todo.md` 2.A's
still-nonexistent `Buffer`/`Tensor` type would otherwise be.

Verified two ways: (1) `kiln/tests/scripts/shared_link_check.sh`
(new -- `--link=shared` had NO automated coverage at all before this)
runs a sample of ordinary tests under both `--link=static` and
`--link=shared` and diffs them, proving the second `DT_NEEDED` and the
combined table didn't disturb kiln's own existing runtime imports; (2)
actually running `matrix_mul` surfaced a real, previously invisible bug:
kiln's process-exit path used the `exit` syscall (60, terminates only
the calling thread) instead of `exit_group` (231, terminates the whole
process) -- invisible for 100% of kiln's history because no kiln
program had ever spawned a second thread, until `ash_gpu_matrix_multiply`'s
OpenMP worker pool did. The program printed its correct answer and then
hung forever on the orphaned worker threads. Fixed in all three exit
sites (`runtime/print_int.c`, `builtins/exit_builtin.c`, `runtime/errors.c`'s
two uncaught-error paths); `exit_group` with only one thread behaves
identically to `exit`, so every existing single-threaded program's
behavior is unchanged (confirmed: `run_regression.sh check` byte-identical
before and after). This is the whole point of actually running
something instead of stopping at "compiles" -- and the Vulkan backend
itself has since actually run too (next section), which is exactly how
the matmul exit-syscall bug and the matmul-loses-on-GPU finding below
were both found: never by reasoning about the code.

## The GPU is real now, and "when it's ok" needs a real answer

This sandbox is WSL2 on the user's own machine, which had GPU
passthrough infrastructure already present but no Vulkan driver
installed (`pacman -S vulkan-dzn mesa` fixed that -- not covered by
`pyre/tools/install_deps.sh`, since it's a GPU driver, not SDK/build
tooling). `ash_gpu_detect()` now finds the real RX 9070 XT for real.
First measurements, same-moment CPU-vs-GPU: `matrix_multiply` is ~2.5x
*slower* on GPU (fp64-on-consumer-GPU, an untiled shader, host-visible
memory, and Dozen being a translation layer all stack against it);
`chaos_iterate` (new kernel, added specifically to find a workload shape
that wins) is ~34x *faster* on GPU. Full numbers and why: `pyre/todo/todo.md`'s
"Real GPU results" section.

Neither dispatcher (`src/C/runtime/device.c`) tells these two cases
apart -- both still unconditionally prefer GPU-when-available, which the
matmul result proves is actively wrong for that kernel today. The "when
it's ok" rules below are therefore still incomplete for Pyre's OWN
dispatch, not just ashvm/kiln's: a real cost model (arithmetic intensity,
data size, whatever the right signal turns out to be) is the next task,
explicitly deferred at the user's request until everything else here is
wrapped up -- not forgotten, and not something to guess at ahead of
having more than two data points.

## What "always ... when it's ok" means in practice

- ashvm: always, unconditionally, once `N >= ASH_GPU_MIN_N` -- no
  separate opt-in, because ashvm never promised zero dependencies.
- kiln: always under `--link=shared` (once `libpyre.so` is installed
  at `KILN_RPATH`, same as `libkilnrt.so`); never under the default
  `--link=static`, which keeps working with nothing installed beyond
  kiln itself, unconditionally, forever.
