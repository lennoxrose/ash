# Ash GPU Acceleration

> **TL;DR:** Ash stays a simple language. Kiln stays the native compiler. A **separate project (`ash-gpu`)** handles GPU work and picks CPU, SIMD, or GPU automatically. No GPU? Program still runs on CPU.

---

## Table of Contents

1. [Goals & Non-Goals](#goals--non-goals)
2. [Why GPUs](#why-gpus)
3. [Architecture](#architecture)
4. [Components](#components)
5. [Dispatch: When to Use the GPU](#dispatch-when-to-use-the-gpu)
6. [Memory & Kernel Fusion](#memory--kernel-fusion)
7. [Language Surface](#language-surface)
8. [Standard Library Acceleration](#standard-library-acceleration)
9. [Tooling](#tooling)
10. [Development Phases](#development-phases)
11. [Design Principles](#design-principles)
12. [Risks & Open Questions](#risks--open-questions)

---

## Goals & Non-Goals

**Goals**

- Ordinary Ash code benefits from GPU hardware without the programmer managing kernels.
- Works on AMD, NVIDIA, and Intel. No vendor lock-in.
- Every program runs without a GPU (CPU fallback).
- Core language and compiler stay small.

**Non-Goals**

- Turning `ashvm` or Kiln into a GPU framework.
- Making Ash a "GPU language" with GPU syntax in everyday code.
- Moving individual scalar math to the GPU (see [below](#do-not-gpu-every-operation)).
- Multi-GPU support in the first versions.

---

## Why GPUs

CPUs (e.g. Ryzen 7 7800X3D) are great at branching, sequential work, and low latency.
GPUs (e.g. RX 9070 XT) are built to run thousands of similar operations at once.

```text
CPU:  work 1 → work 2 → work 3 → work 4 → ...

GPU:  work 1 ─┐
      work 2 ─┤
      work 3 ─┼──> thousands in parallel
      work 4 ─┤
      work 5 ─┘
```

**Good fits:** array/matrix ops, image processing, simulations, signal processing, physics, procedural generation, fractals, particle systems, large sorts/reductions/searches, ML workloads, parallel data processing.

**Catch:** launching GPU work and moving data CPU ↔ GPU both cost time. So the system must *decide* whether the GPU is worth it, not assume it.

---

## Architecture

```text
              Ash Program
                   │
                   ▼
            Kiln / Ash IR
                   │
                   ▼
          Optimization Layer
                   │
        ┌──────────┴──────────┐
        ▼                     ▼
    CPU path              GPU path
 (native / SIMD)       (ash-gpu runtime)
                              │
                 ┌────────────┼────────────┐
                 ▼            ▼            ▼
               Vulkan        HIP         CUDA
              (all GPUs)    (AMD)      (NVIDIA)
```

**Responsibilities**

| Layer | Job |
|-------|-----|
| **Ash** | Describes *what* the programmer wants |
| **Kiln** | Turns it into efficient native code |
| **ash-gpu** | Hardware-aware acceleration for heavy compute |

Kiln talks to `ash-gpu` through a **small C ABI** and never needs to know backend internals.

---

## Components

Proposed layout:

```text
ash-gpu/
├── include/
│   └── ash_gpu.h        # public C ABI
├── runtime/             # device, memory, dispatch, scheduler (C++)
├── backends/            # vulkan, hip, cuda, cpu
├── kernels/             # vector, matrix, reduction, sorting, image
├── bindings/ash/        # Ash-facing API
├── tools/               # info, benchmark, profile, doctor
├── benchmarks/
└── tests/
```

### 1. Hardware Detection

Detect what's available and report it:

```text
CPU:  AMD Ryzen 7 7800X3D
GPU:  AMD Radeon RX 9070 XT
VRAM: 16 GB
Compute: available
```

Collected info: vendor, model, VRAM, compute units, supported APIs, precision formats, features, driver capabilities. No suitable GPU → everything falls back to CPU.

### 2. Backends

Never tie Ash itself to one API.

| Backend | Notes |
|---------|-------|
| **Vulkan Compute** | Cross-vendor (AMD/NVIDIA/Intel), mature, explicit control. Best default. |
| **HIP** | CUDA-like model, strong on AMD |
| **CUDA** | Highly optimized NVIDIA path |
| **DirectCompute** | Possible Windows-specific option |
| **CPU** | Always-present fallback |

### 3. Optimized Kernels

```text
kernels/
├── arithmetic/   vector_add, vector_mul, vector_fma
├── vector/       vector_sin, vector_cos
├── matrix/       matrix_mul
├── reduction/    reduce_sum, reduce_min, reduce_max
├── sorting/      sort, prefix_sum
├── image/
├── signal/
└── simulation/
```

Written in whatever gives the best performance: C++, HIP, CUDA, Vulkan/SPIR-V shaders. The goal is speed, not writing everything in Ash.

### 4. Why C++ for the Runtime

- RAII for GPU resources
- Cleaner device/backend abstractions
- Better ecosystem support
- Easier async management

Kiln stays in C. The boundary is a plain C ABI:

```c
bool  ash_gpu_available(void);
void *ash_gpu_alloc(size_t bytes);
void  ash_gpu_free(void *buf);
int   ash_gpu_upload(void *dst, const void *src, size_t bytes);
int   ash_gpu_download(void *dst, const void *src, size_t bytes);
int   ash_gpu_dispatch(/* kernel, args, size */);
```

```text
Ash → C ABI → C++ runtime → Vulkan / HIP / CUDA / DirectX
```

### 5. Async Execution

GPUs are async by nature. Don't block unless you must:

```text
CPU ──► launch GPU work
    ──► keep doing CPU work
    ──► launch more GPU work
    ──► sync only when the result is needed
```

### 6. Multiple GPUs *(future)*

Split big workloads across devices, e.g. 100M elements → 60M on GPU 0, 40M on GPU 1. Not an initial requirement.

---

## Dispatch: When to Use the GPU

The runtime picks the best implementation per operation:

```text
operation
   │
   ├─ small workload?  → CPU
   ├─ medium workload? → SIMD CPU
   └─ huge workload?   → GPU
```

| Example | Elements | Choice |
|---------|----------|--------|
| vector multiply | 10 | CPU |
| vector multiply | 1,000 | CPU / SIMD |
| vector multiply | 128 | CPU SIMD |
| vector multiply | 10,000,000+ | GPU |

Thresholds depend on: operation type, element count, memory size, GPU characteristics, transfer cost, CPU SIMD capability, and **previous benchmark data**.

### Do Not GPU Every Operation

```ash
// BAD: overhead dwarfs the work
let x = gpu 10 * 20;

// GOOD: big batch, GPU can shine
for i in 0..100000000 {
    output[i] = input[i] * 20;
}
```

Optimize **workloads**, not individual instructions.

---

## Memory & Kernel Fusion

### Keep data on the GPU

Bad (ping-pong):

```text
CPU → GPU → CPU → GPU → CPU
```

Good (upload once, download once):

```text
CPU ─upload─► GPU memory ─► multiply ─► add ─► sin ─► normalize ─download─► CPU
```

### Kernel fusion

Instead of three kernels:

```ash
a = b * 2
c = a + 10
d = sin(c)
```

generate one:

```text
d[i] = sin(b[i] * 2 + 10)
```

Saves: kernel launch overhead, intermediate writes, intermediate reads, synchronization.

---

## Language Surface

**Primary goal: automatic.** Plain Ash code gets optimized.

```ash
let a = huge_array();
let b = huge_array();

for i in 0..a.length {
    a[i] = a[i] * b[i] + 42;
}
```

The toolchain recognizes this as a highly parallel map.

### Vectorized operations

High-level ops that are easy to analyze:

```ash
a = a * 2
c = a + b
result = sin(values)
```

Internally: `parallel map → parallel map → parallel map` → fusable into one pipeline.

### Explicit control *(advanced users, later)*

Syntax TBD. Possibilities:

```ash
parallel {
    for i in 0..values.length {
        values[i] = values[i] * 2;
    }
}

gpu {
    for i in 0..values.length {
        output[i] = input[i] * input[i];
    }
}
```

| Mode | Behavior |
|------|----------|
| Normal Ash | Automatic optimization |
| `parallel` / `gpu` blocks | Programmer explicitly requests acceleration |

---

## Standard Library Acceleration

Likely the biggest practical win. Users call normal APIs; the implementation picks CPU / SIMD / GPU.

```text
array.sort()   array.sum()   array.min()    array.max()
array.map()    array.filter()
matrix.multiply()
image.resize() image.blur()
```

### Example

```ash
let pixels = image.load("image.png");

pixels = pixels.blur();
pixels = pixels.color_correct();
pixels = pixels.resize(3840, 2160);

image.save(pixels, "output.png");
```

| Step | Runs on |
|------|---------|
| `image.load` | CPU |
| `blur` | GPU |
| `color_correct` | GPU |
| `resize` | GPU |
| `image.save` | CPU |

The whole middle pipeline stays resident in GPU memory: one upload, one download.

---

## Tooling

```text
ash-gpu info        # detected devices, backend, VRAM, status
ash-gpu benchmark   # measure CPU vs SIMD vs GPU
ash-gpu kernels     # inspect available kernels
ash-gpu doctor      # diagnose driver/setup problems
ash-gpu compile     # compile GPU kernels
ash-gpu profile program.ash
```

Sample `info` output:

```text
Ash GPU Runtime

Backend: Vulkan
Device:  AMD Radeon RX 9070 XT
VRAM:    16 GB
Status:  Available
```

Sample `profile` output:

```text
Ash GPU Profile

CPU time:             421 ms
GPU time:              83 ms
GPU transfer time:     19 ms
Kernel launch time:     3 ms
GPU utilization:       91%

Top kernels:
    vector_mul     31 ms
    matrix_mul     24 ms
    sin_kernel     17 ms
    reduction       8 ms
```

The profiler answers the key question: **is the GPU actually helping?**

---

## Development Phases

| Phase | Goal | Details |
|-------|------|---------|
| **1. Standalone runtime** | Prove the GPU path works | Detection, one backend, alloc, upload/download, kernel execution, CPU fallback, benchmarks. **Don't touch Ash yet.** |
| **2. Primitive library** | Fast building blocks | Vector math, matrix math, reductions, sorting, image processing. Benchmark-driven. |
| **3. Ash bindings** | Let users experiment | Small API like `gpu.available()`, `gpu.array(...)`. No compiler changes. |
| **4. Kiln integration** | Compiler awareness | Only after the runtime is solid. Kiln emits calls to `ash-gpu` for suitable workloads. |
| **5. Automatic acceleration** | The real payoff | Plain `for` loops over large arrays become GPU workloads automatically. |

---

## Design Principles

1. **GPU is optional.** No GPU means "use the best CPU implementation", never "can't run".
2. **Never assume the GPU is faster.** Only use it when the workload justifies it.
3. **Minimize transfers.** Keep data on the GPU across chained operations.
4. **Optimize whole workloads**, not single arithmetic instructions.
5. **Keep Ash simple.** GPU complexity lives outside the language.
6. **Keep Kiln maintainable.** No giant GPU framework inside the compiler.
7. **Benchmark everything.** Measurements over assumptions.

---

## Risks & Open Questions

- **Floating-point consistency:** GPU and CPU results may differ slightly. Define what Ash guarantees.
- **Detecting parallel-safe loops:** Aliasing and loop-carried dependencies make auto-parallelization hard. Start with a restricted, provably safe subset.
- **Shipping binaries:** A program compiled on a GPU machine runs on a laptop. Needs runtime detection (not compile-time), plus bundled or JIT-compiled kernels.
- **Driver/backend variance:** Vulkan compute behavior differs across vendors. Needs a solid test matrix.
- **Dispatch accuracy:** Heuristics may misjudge. Benchmark data / calibration at install time could help.
- **Explicit syntax:** `parallel {}` vs `gpu {}` vs a library API. Decide later.
- **Error handling:** What happens on GPU out-of-memory or device loss mid-run? Should fall back to CPU transparently.

---

## The Big Idea

```text
Ash      → simple high-level programs
Kiln     → optimized native execution
Ash GPU  → hardware-aware acceleration
              │
              ▼
        CPU / SIMD / GPU
```

Ash doesn't become a GPU language. Kiln doesn't become a GPU compiler framework. A normal Ash program can get dramatically faster on machines with capable GPUs and still run fine on CPU-only machines.

GPU acceleration becomes an **implementation detail of the ecosystem**, not something every Ash programmer has to think about.