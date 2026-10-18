# TODO: Kiln, Ash, and Pyre Hardware Acceleration Roadmap

## Objective
Enable full-scale matrix operations ($500 \times 500$ and beyond) without hitting memory ceilings or segfaults, update the runtime engines (`kiln` and `ashvm`), and establish the necessary language features in Ash for **Pyre** (`ash-gpu`) to deliver massive GPU acceleration.

---

## 1. Kiln Runtime & Compiler Upgrades

### A. Memory Management
- [x] **Remove Hardcoded Heap Cap:**
  - Replace static `static char heap[16 * 1024 * 1024]` allocation in Kiln's native runtime generator with dynamic OS page allocations.
  - **Linux:** Implement `mmap` (`sys_mmap`, syscall 9 on x86-64) or `brk` to request pages dynamically when heap memory is exhausted.
  - **Windows:** Emit `VirtualAlloc` / `HeapAlloc` imports via `kernel32.dll`.
  - Done 2026-10-08: grows in 64MB chunks (`brk` again on Linux, commits more of an upfront 4GB `MEM_RESERVE` on Windows) instead of one fixed grab -- see CHANGELOG. `tests/cases/memory/heap_growth.ash`.
- [x] **Arena / Temporary Region Allocators:**
  - Implement checkpoint-based region allocation (`heap_checkpoint()` / `heap_reset()`) inside loop scopes so short-lived inner-loop temporaries don't exhaust memory when GC isn't active.
  - Done 2026-10-08, partially: `heap_emit_checkpoint`/`heap_emit_reset` exist and are verified (see CHANGELOG), but are NOT wired into general `each`/`during` loop bodies -- that needs an escape analysis kiln's single-pass compiler doesn't have yet (see goals.md item 1), and an unsound version would corrupt memory rather than just leak it. They're ready for Pyre's own lowering to call directly, where the generated loop's shape is fully under kiln's own control instead of arbitrary user code.

### B. Compiler AST & Syntax Support
- [x] **Chained Multi-Index Assignment:**
  - Update the parser and code emitter in `Kiln` (`src/compiler/`) to support nested array index mutation (`C[i][j] = val`).
  - Emit direct dereference chains rather than requiring manual intermediate extraction (`local row = C[i]; row[j] = val; C[i] = row;`).
  - Done 2026-10-08: see CHANGELOG. `tests/cases/collections/chained_index_assign.ash`.

---

## 2. Ash Language Extensions (Required for Pyre)

Even if these do not yet exist in Ash, define the grammar and compiler IR to support them:

### A. Flat & Contiguous Memory Types
- [ ] **Contiguous Buffer Type (`Buffer` / `Tensor`):**
  - Add native contiguous 1D/2D array buffers (e.g., `Buffer.alloc(type, dimensions)` or `Tensor([N, N])`).
  - Avoid storing matrices as nested pointer arrays (`Array<Array<T>>`) to enable zero-copy GPU transfers and $O(1)$ flat indexing ($i \cdot N + j$).

### B. Structural Loop Construct & Parallel Map Hints
- [ ] **Parallel Loop / Acceleration Annotations:**
  - Add language constructs or attributes indicating data independence for parallel dispatch:
    ```ash
    parallel for i in 0..N { ... }
    ```
  - Or vector-level primitives:
    ```ash
    local C = matrix_mul(A, B);
    ```

### C. Foreign Function Interface (FFI) & C ABI Binding
- [x] **C ABI Binding Primitive, for one specific function:**
  - Add native low-level foreign function bindings (`@extern` or `@import_c`) so Ash runtime scripts can pass buffer pointers directly to Pyre's C ABI layer (`ash_gpu_dispatch`, `ash_gpu_upload`, `ash_gpu_download`).
  - Done 2026-10-08, narrowly: not a general `@extern`/`@import_c` primitive (no Ash-level syntax for calling ARBITRARY C functions exists) -- instead, `kiln`'s `matrix_mul(a, b)` builtin (`codegen/C/collections/matrix_mul.c`) calls `ash_gpu_matrix_multiply` specifically, through the GOT slot `elf/C/elf_dynamic.c`'s `--link=shared` writer lays out for it. A real general FFI primitive is still open; this is the one concrete binding that exists.

---

## 3. Pyre (`ash-gpu`) Integration Architecture

- [ ] **Data Residency Pipeline:**
  - Enforce upload-once, download-once semantics: keep intermediate computation results in VRAM across chained standard library calls (`blur()`, `matrix_mul()`, `resize()`).
  - Not started -- there is no GPU backend yet for anything to be resident on (see below).
- [~] **Vulkan / HIP / CUDA Runtime Dispatch:**
  - Detect compute hardware at launch (`ash_gpu_available()`).
  - Automatically route small matrix tasks ($N < 32$) to CPU SIMD and large matrix tasks ($N \ge 128$) to Pyre compute pipelines.
  - `ash_gpu_available()` really tries Vulkan (`ash_gpu_vulkan_init()`, `src/C/backends/vulkan/context.cpp`) and, as of 2026-10-08, actually succeeds: this sandbox is WSL2, which has GPU passthrough infrastructure (`/dev/dxg`, Microsoft's D3D12 translation libraries) but no Vulkan driver installed by default. Installing `vulkan-dzn` (Mesa's Vulkan-over-D3D12 driver) made `vulkaninfo` and `ash_gpu_detect()` both find the user's real **AMD Radeon RX 9070 XT** (reported as "Microsoft Direct3D12 (AMD Radeon RX 9070 XT)", driver `DRIVER_ID_MESA_DOZEN`, `shaderFloat64 = true`). `pyre/tools/install_deps.sh` doesn't cover this -- it's a GPU driver, not SDK/build tooling; install it by hand (`pacman -S vulkan-dzn mesa`) on any other WSL2 machine that needs it. First real execution of the Vulkan path, ever -- see the benchmark table below for what it found. HIP/CUDA: not started. Size-based routing ($N<32$/$N\ge128$) doesn't exist at the Pyre level -- `ash_gpu_matrix_multiply` and `ash_gpu_chaos_iterate` both dispatch purely on backend availability, not size or any measured cost, for every workload; the one size threshold that exists ($\text{ASH\_GPU\_MIN\_N}=64$) is in ashvm's builtin, not here. **This is exactly the gap flagged below as the next real task** (see "Smart dispatch").
- [x] **Fallback Mechanism (for the kernels that exist so far):**
  - On systems with no dedicated GPU or incompatible drivers, transparently fall back to multi-threaded CPU routines without raising runtime errors.
  - Done 2026-10-08 for `ash_gpu_matrix_multiply` and `ash_gpu_monte_carlo_risk`: both always run on the CPU backend today (ikj-reordered + OpenMP for matmul; per-thread-seeded OpenMP for the Monte Carlo kernel), which is also exactly what "fallback" means once a real GPU path exists later.

### Phase 1 benchmarks (pyre/benchmarks/) -- CPU backend only, no GPU
Both of the user's benchmark challenges ("beat the C++ reference by at least 50%, or the solution isn't allowed"), run fresh on this machine, CPU-only (`make benchmark` in `pyre/`):

| Benchmark | C++ reference | Pyre (CPU backend) | Reduction |
|---|---|---|---|
| `matrix_multiply` (2000x2000) | 5.532s (`g++ -O2`) | 0.223-0.248s | ~95.6% (target: 50%) |
| `monte_carlo_no_pi` (1e9 iters) | 1.840s (`g++ -O3`) | 0.231-0.246s | ~87% (target: 50%) |

Both cleared the bar without touching a GPU at all: `matrix_multiply` from the ijk-vs-ikj loop reorder (fixes the reference's cache-hostile strided access to `B[k][j]`) plus splitting rows across this machine's 8 cores with OpenMP; `monte_carlo_no_pi` from giving each of the 8 threads its own splitmix32-derived seed stream instead of one sequential chain. `pyre/tests/{matrix_multiply,monte_carlo}_test.c` check correctness (a from-scratch scalar reference for matmul; self-consistency plus an analytic sanity range for the Monte Carlo probability), independent of the benchmark numbers above.

### Ash bindings and kiln integration (ideas/gpu_acceleration.md Phases 3 and 4) -- both done
`matrix_mul(a, b)` exists as a plain builtin on `ashvm` (`ashvm/src/builtins/C/matrix.c`), calling `ash_gpu_matrix_multiply` directly for square arrays-of-arrays of numbers at or above `ASH_GPU_MIN_N = 64`, a local C loop below it. `ashvm/tests/cases/collections/matrix_mul.ash` covers both paths plus the error cases.

`kiln` now has the same builtin (`codegen/C/collections/matrix_mul.c`), available only under `--link=shared`: it really calls through the GOT into `libpyre.so` (a second `DT_NEEDED` added to `elf/C/elf_dynamic.c`'s writer, folded into the same combined hash/dynsym/GOT/rela table as kiln's own runtime imports), not a native reimplementation -- see ideas/assigned.md for the full mechanism, including the SysV ABI call wrapper (`codegen/C/platform/linux_call.c`) kiln never needed before this (its own hand-rolled routines are alignment-agnostic; a real gcc/g++-compiled function with OpenMP/SSE isn't). Under `--link=static`, `matrix_mul` is a clear compile error, not a silent no-op -- that mode's whole point is staying dependency-free. `kiln/tests/scripts/shared_link_check.sh` (new) covers both the general `--link=shared` path (previously untested at all) and `matrix_mul` specifically.

Actually running this found a real, previously invisible kiln bug: its process-exit path used syscall 60 (`exit`, kills only the calling thread) instead of 231 (`exit_group`, kills the whole process) -- harmless for kiln's entire history until `ash_gpu_matrix_multiply`'s OpenMP worker threads gave a kiln-generated program a second thread for the first time ever. The program printed the right answer and then hung forever. Fixed in all three exit sites; confirmed behavior-identical for every existing (single-threaded) program via `run_regression.sh`.

### Real GPU results, now that one exists (2026-10-08)
Same-moment, forced-CPU-vs-real-GPU comparisons on the RX 9070 XT (via WSL2/Dozen -- itself a translation layer, "not a conformant Vulkan implementation, testing use only" per Mesa's own warning):

| Kernel | Shape | CPU | GPU | Result |
|---|---|---|---|---|
| `matrix_multiply` (2000x2000) | memory-bound, naive untiled shader, fp64, host-visible memory | 0.44s | 1.07-1.11s | **GPU loses, ~2.5x slower** |
| `chaos_iterate` (new -- 2M elements x 4000 iterations, 8 billion ops) | compute-bound, trivial data movement, no cross-element dependency | ~14.5s | ~0.43s | **GPU wins, ~34x faster** |

`chaos_iterate` (`ash_gpu.h`, `src/C/backends/cpu/chaos.c`, `src/C/backends/vulkan/chaos.cpp` + `chaos.cpp`, `src/shaders/chaos.comp`, benchmarked by `pyre/benchmarks/chaos/run_benchmark.c`) exists specifically to answer "is there ANY workload shape this GPU setup actually wins on" after `matrix_multiply`'s real measurement came back a loss -- same recurrence (`x = sqrt(abs(x)+1)*0.99999+0.00001`, chosen because GLSL's `GL_ARB_gpu_shader_fp64` extension gives `double` overloads for `sqrt`/`abs`/arithmetic but NOT `sin`/`cos`, discovered by trying it) on both backends, results matching exactly (`max |CPU - GPU| = 0`) across all 2M elements.

The two results together are the concrete evidence for the next task, not a contradiction: `matrix_multiply` is memory/transfer-bound and specifically hurt by fp64-on-consumer-GPU plus an untiled shader; `chaos_iterate` is compute-bound enough that none of that matters. **Neither dispatcher (`src/C/runtime/device.c`) currently tells the difference** -- both unconditionally prefer GPU-when-available with no cost model at all.

### Smart dispatch: done 2026-10-09 (the manager)
`src/C/runtime/manager.c` places every call from a table of what this machine was actually seen to do at that workload size, learned online from real calls -- no calibration pass, nothing machine-specific shipped, no call ever waits for a GPU to start (background warm-up while the CPU runs). See CHANGELOG 2026-10-09 and `pyre help`. Open: (1) a process that makes a single heavy call exits before the GPU is up, so it never learns the GPU -- `pyre train` covers that; splitting one big call between CPU and GPU (chunked, concurrent) would use the GPU inside that first call too. (2) A size where the GPU lost is not retried later (no re-exploration of lost buckets); `pyre train --reset` or deleting the profile clears it. (3) ashvm's `ASH_GPU_MIN_N = 64` guard is still a fixed first guess; the manager could replace it now.

### Dashboard (2026-10-09)
`pyre dashboard` (http://127.0.0.1:1198, offline): source `pyre/src/web/`, built to `bin/extensions/pyre/web/`. Not done: authentication beyond the localhost/Host/Origin checks (it is a local tool), a live per-call feed of the manager's decisions in a running program (`PYRE_TRACE=1` still covers that in the terminal), several concurrent sessions (one at a time by design).

### Tooling
`pyre/tools/install_deps.sh` installs the Vulkan SDK pieces the Vulkan backend needs to build (headers, loader, `glslc`/`glslangValidator`, spirv-tools) -- Arch/pacman only so far, matches both this sandbox and the user's own machine. It installs the SDK, not a GPU driver; `vulkaninfo` still correctly reports no device on a machine with none.