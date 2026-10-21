#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "builtins/H/internal.h"
#include "vm/H/vm.h"

// Raw device access for libraries written in Ash (a window lib talks to the X server over a socket it opens
// itself). Addresses and syscall numbers are plain numbers: a double holds every user-space address (47 bits).
// There is no checking -- a wrong address crashes the process, like the C it stands for. Linux only; same
// contract as kiln's, which has none of these yet.
static int64_t arg_i64(VMValue v, const char *what) {
    if (v.type != VM_NUM) { vm_runtime_error("%s expects numbers\n", what); }
    return (int64_t)v.number;
}

VMValue vm_call_builtin_raw(int id, VMValue *args, int argc) {
    switch (id) {
        case 58: { // raw_syscall(n, a1..a6) -- the kernel's result, or negative errno
            if (argc != 7) { vm_runtime_error("raw_syscall(n, a1, a2, a3, a4, a5, a6) expected\n"); }
            long a[7];
            for (int i = 0; i < 7; i++) a[i] = (long)arg_i64(args[i], "raw_syscall");
            fflush(stdout); // a write(1, ...) must not overtake buffered output
            long r = syscall(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
            return vm_num(r < 0 ? -errno : (double)r);
        }
        case 59: { // raw_alloc(n) -- address of n zeroed bytes, 0 on failure
            if (argc != 1) { vm_runtime_error("raw_alloc(n) expected\n"); }
            int64_t n = arg_i64(args[0], "raw_alloc");
            return vm_num(n < 0 ? 0 : (double)(uintptr_t)calloc(1, n ? (size_t)n : 1));
        }
        case 60: { // raw_free(addr)
            if (argc != 1) { vm_runtime_error("raw_free(addr) expected\n"); }
            free((void *)(uintptr_t)arg_i64(args[0], "raw_free"));
            return vm_nil();
        }
        case 61: { // raw_peek(addr, width) -- width 1, 2, 4 or 8 bytes, unsigned, little-endian
            if (argc != 2) { vm_runtime_error("raw_peek(addr, width) expected\n"); }
            const void *p = (const void *)(uintptr_t)arg_i64(args[0], "raw_peek");
            switch (arg_i64(args[1], "raw_peek")) {
                case 1: return vm_num(*(const uint8_t *)p);
                case 2: { uint16_t v; memcpy(&v, p, 2); return vm_num(v); }
                case 4: { uint32_t v; memcpy(&v, p, 4); return vm_num(v); }
                case 8: { uint64_t v; memcpy(&v, p, 8); return vm_num((double)v); }
            }
            vm_runtime_error("raw_peek width must be 1, 2, 4 or 8\n");
        }
        case 62: { // raw_poke(addr, width, value) -- the low width bytes of value, little-endian
            if (argc != 3) { vm_runtime_error("raw_poke(addr, width, value) expected\n"); }
            void *p = (void *)(uintptr_t)arg_i64(args[0], "raw_poke");
            int64_t v = arg_i64(args[2], "raw_poke");
            switch (arg_i64(args[1], "raw_poke")) {
                case 1: *(uint8_t *)p = (uint8_t)v; return vm_nil();
                case 2: { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); return vm_nil(); }
                case 4: { uint32_t x = (uint32_t)v; memcpy(p, &x, 4); return vm_nil(); }
                case 8: memcpy(p, &v, 8); return vm_nil();
            }
            vm_runtime_error("raw_poke width must be 1, 2, 4 or 8\n");
        }
        case 63: { // raw_fill(addr, value32, count) -- count 32-bit words, the low 32 bits of value (a pixel row)
            if (argc != 3) { vm_runtime_error("raw_fill(addr, value32, count) expected\n"); }
            uint32_t *p = (uint32_t *)(uintptr_t)arg_i64(args[0], "raw_fill");
            uint32_t v = (uint32_t)arg_i64(args[1], "raw_fill");
            int64_t n = arg_i64(args[2], "raw_fill");
            for (int64_t i = 0; i < n; i++) p[i] = v;   // the compiler turns this into rep stos / SIMD stores
            return vm_nil();
        }
        case 64: { // raw_copy(dst, src, n) -- n bytes, the ranges may overlap (memmove)
            if (argc != 3) { vm_runtime_error("raw_copy(dst, src, n) expected\n"); }
            void *dst = (void *)(uintptr_t)arg_i64(args[0], "raw_copy");
            const void *src = (const void *)(uintptr_t)arg_i64(args[1], "raw_copy");
            int64_t n = arg_i64(args[2], "raw_copy");
            if (n > 0) memmove(dst, src, (size_t)n);
            return vm_nil();
        }
    }
    vm_runtime_error("unknown builtin id: %d\n", id);
}
