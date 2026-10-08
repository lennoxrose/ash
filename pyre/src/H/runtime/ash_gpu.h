#ifndef ASH_GPU_H
#define ASH_GPU_H
/*
 * Pyre's public C ABI (ideas/gpu_acceleration.md's "Components" section:
 * a small C ABI, Kiln never needs to know backend internals). Phase 1
 * ("standalone runtime... don't touch Ash yet") -- nothing here is wired
 * into kiln or ashvm yet, this is the foundation that integration will
 * later call into.
 *
 * C linkage throughout, even though backends may be implemented in C++
 * internally (the ideas doc's own "Why C++ for the Runtime" section),
 * so kiln's codegen (plain C, no name mangling to reason about) and any
 * other C caller can link against this header unmodified.
 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ASH_GPU_BACKEND_NONE, /* no usable GPU -- every call below runs on CPU */
    ASH_GPU_BACKEND_VULKAN,
    ASH_GPU_BACKEND_HIP,
    ASH_GPU_BACKEND_CUDA
} AshGpuBackend;

typedef struct {
    AshGpuBackend backend;
    char device_name[128]; /* "CPU fallback" when backend == ASH_GPU_BACKEND_NONE */
    unsigned long long vram_bytes; /* 0 when there is no discrete device */
} AshGpuInfo;

/* Really tries to bring up a Vulkan device (src/C/runtime/device.c calls
 * src/C/backends/vulkan/context.cpp's ash_gpu_vulkan_init(), not just a
 * loader-presence check) and reports what it finds. This sandbox turns
 * out to be WSL2 with GPU passthrough available but no Vulkan driver
 * installed at first (vulkaninfo: "Found no drivers!"); installing
 * vulkan-dzn (Mesa's Vulkan-over-D3D12 driver, pyre/tools/install_deps.sh
 * doesn't cover it -- it's GPU-driver-specific, not SDK/build tooling)
 * made ash_gpu_detect() report ASH_GPU_BACKEND_VULKAN for real, naming
 * the user's actual AMD Radeon RX 9070 XT. Never fails: "no GPU" is a
 * normal, fully supported answer, never an error (design principle 1 in
 * ideas/gpu_acceleration.md). */
AshGpuInfo ash_gpu_detect(void);

/* True iff ash_gpu_detect() found a usable (non-CPU-fallback) device. */
int ash_gpu_available(void);

/*
 * The manager's answer to "would you like me to take this over?", without
 * doing the work. Every kernel call below asks the same question itself and
 * acts on the answer (src/C/runtime/manager.c); these let a caller ask first,
 * e.g. to skip converting its own data into flat buffers for a job Pyre
 * would only hand straight back to the CPU.
 *
 * The prediction comes from what this machine has actually been seen to do
 * at this workload size, learned from real calls (no calibration pass, no
 * fixed threshold, nothing shipped; see H/runtime/manager.h). `backend` is
 * where Pyre would run it right now (ASH_GPU_BACKEND_NONE = the CPU
 * backend); est_seconds is the predicted time there and est_cpu_seconds the
 * CPU backend's, both negative until that size has been measured. A GPU
 * that is not up yet is never chosen unless the saving is known to beat
 * its start-up; calls never wait for a start-up in flight.
 */
typedef struct {
    AshGpuBackend backend;
    double est_seconds;
    double est_cpu_seconds;
} AshGpuPlan;

AshGpuPlan ash_gpu_plan_matrix_multiply(int n);
AshGpuPlan ash_gpu_plan_chaos_iterate(int n, int iterations);
AshGpuPlan ash_gpu_plan_monte_carlo_risk(long long iterations);

/*
 * C = A * B, all three N x N, row-major, contiguous (flat, not an array
 * of row pointers -- see todo.md 2.A's "Contiguous Buffer Type": this is
 * the layout a future Ash-level Buffer/Tensor type would hand across
 * this exact boundary with zero copying). C must not alias A or B.
 *
 * Dispatches to whatever ash_gpu_detect() found (src/C/runtime/device.c):
 * Vulkan (src/C/backends/vulkan/matrix_multiply.cpp, a straightforward
 * one-thread-per-output-element compute shader -- now verified to
 * actually run, on the RX 9070 XT via WSL2's D3D12 passthrough) if
 * available, else the CPU backend (src/C/backends/cpu/matrix_multiply.c:
 * cache-friendly loop order plus OpenMP, no GPU required at all --
 * design principle 1: no GPU means "best CPU implementation", never
 * "can't run"). Measured result, same hardware: the GPU path is
 * currently ~2.5x SLOWER than the CPU path for this kernel (fp64
 * throughput on a consumer GPU, no workgroup tiling, host-visible
 * memory instead of device-local, and the Dozen driver itself is a
 * translation layer, not native) -- see ash_gpu_chaos_iterate below for
 * a workload shaped so the GPU actually wins, and ideas/assigned.md for
 * why this function still unconditionally prefers GPU-when-available
 * despite that (not yet fixed to measure first).
 */
void ash_gpu_matrix_multiply(const double *a, const double *b, double *c, int n);

/*
 * The second benchmark kernel (pyre/benchmarks/monte_carlo_no_pi): run
 * `iterations` draws of the same two-draw-per-iteration LCG recurrence
 * as mcnp.cpp's reference, count how many exceed `threshold`, and
 * report both the raw count and the estimated probability.
 *
 * The CPU backend splits `iterations` across threads with independent
 * per-thread seed streams (see src/C/backends/cpu/monte_carlo.c) -- the exact
 * sequence of draws, and so the exact risky count, therefore differs
 * from a single-threaded run with the same seed. The benchmark's own
 * target is wall-clock time, not bit-for-bit reproduction of one
 * particular single-threaded trajectory.
 */
void ash_gpu_monte_carlo_risk(long long iterations, double threshold, unsigned int seed,
                               long long *out_risky, double *out_probability);

/*
 * In place: data[i] = iterate(data[i]) for each of the n elements,
 * where iterate() applies x = sin(x) + cos(x * 1.5), `iterations`
 * times. Bounded (sin+cos of a bounded argument never escapes roughly
 * [-2, 2]), so it runs indefinitely without overflow -- purely a
 * workload shape, not a model of anything.
 *
 * Deliberately the OPPOSITE shape from matrix_multiply: huge compute
 * per element (iterations can be thousands), trivial data movement
 * (one double in, one double out, no cross-element dependency, no
 * tiling to get right). Added specifically because matrix_multiply's
 * real, measured GPU result was a loss (see that function's comment) --
 * this is the workload ideas/gpu_acceleration.md's own dispatch
 * principle describes as GPU-favorable ("huge workload" with high
 * arithmetic intensity, where per-dispatch overhead and fp64-throughput
 * cost get amortized over thousands of iterations instead of dominating
 * a handful). Same dispatch rule as matrix_multiply: GPU if
 * ash_gpu_available(), else CPU (src/C/backends/cpu/chaos.c, OpenMP
 * across elements) -- never "can't run".
 */
void ash_gpu_chaos_iterate(double *data, int n, int iterations);

#ifdef __cplusplus
}
#endif
#endif
