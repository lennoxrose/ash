#include <stdlib.h>
#include "builtins/H/internal.h"
#include "vm/H/vm.h"
#include "H/runtime/ash_gpu.h"

// matrix_mul(a, b) -- a and b must be square N x N arrays of arrays of
// numbers, same N (general M x K times K x P multiplication is a known
// gap: Pyre's own kernel, pyre/src/H/runtime/ash_gpu.h, only takes one N today
// -- documented in pyre/todo/todo.md rather than silently assumed away).
//
// "Always use Pyre when it's ok to" (ideas/assigned.md): ashvm already
// links against libc/libm/OpenMP normally (unlike kiln's hand-rolled,
// zero-dependency ELF output), so taking on Pyre as a build-time
// dependency here isn't the architecture problem it would be for kiln --
// see assigned.md for why kiln's own matrix_mul (codegen/C/collections/matrix_mul.c)
// instead ports the same loop-order technique natively rather than
// linking against libpyre.
//
// Below ASH_GPU_MIN_N, a plain local loop skips Pyre entirely: OpenMP's
// own thread-launch overhead would dominate a handful of multiply-adds,
// exactly the "don't accelerate every operation, only workloads that
// justify it" principle (design principle 2) ash_gpu.h itself
// documents for picking a backend.
#define ASH_GPU_MIN_N 64

static int matrix_square_n(VMValue m) {
    if (m.type != VM_ARRAY || m.array->count == 0) return -1;
    int n = m.array->count;
    for (int i = 0; i < n; i++) {
        VMValue row = m.array->items[i];
        if (row.type != VM_ARRAY || row.array->count != n) return -1;
        for (int j = 0; j < n; j++) {
            if (row.array->items[j].type != VM_NUM) return -1;
        }
    }
    return n;
}

static void flatten(VMValue m, int n, double *out) {
    for (int i = 0; i < n; i++) {
        VMValue row = m.array->items[i];
        for (int j = 0; j < n; j++) out[i * n + j] = row.array->items[j].number;
    }
}

static VMValue unflatten(const double *flat, int n) {
    VMArray *result = vm_array_new();
    for (int i = 0; i < n; i++) {
        VMArray *row = vm_array_new();
        for (int j = 0; j < n; j++) vm_array_push(row, vm_num(flat[i * n + j]));
        vm_array_push(result, vm_array_val(row));
    }
    return vm_array_val(result);
}

static void naive_multiply_small(const double *a, const double *b, double *c, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) sum += a[i * n + k] * b[k * n + j];
            c[i * n + j] = sum;
        }
    }
}

VMValue vm_call_builtin_matrix(int id, VMValue *args, int argc) {
    (void)id; // only one builtin in this file so far
    if (argc != 2) vm_runtime_error("matrix_mul(a, b) expected\n");
    int n = matrix_square_n(args[0]);
    int n2 = matrix_square_n(args[1]);
    if (n == -1 || n2 == -1 || n != n2) {
        vm_runtime_error("matrix_mul(a, b) expected two square arrays of arrays of numbers, same size\n");
    }

    double *a = malloc((size_t)n * n * sizeof(double));
    double *b = malloc((size_t)n * n * sizeof(double));
    double *c = malloc((size_t)n * n * sizeof(double));
    flatten(args[0], n, a);
    flatten(args[1], n, b);

    if (n >= ASH_GPU_MIN_N) {
        ash_gpu_matrix_multiply(a, b, c, n);
    } else {
        naive_multiply_small(a, b, c, n);
    }

    VMValue result = unflatten(c, n);
    free(a);
    free(b);
    free(c);
    return result;
}
