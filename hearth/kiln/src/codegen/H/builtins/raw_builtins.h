#ifndef KILN_RAW_BUILTINS_H
#define KILN_RAW_BUILTINS_H

// Raw device access for libraries written in Ash; same contract as ashvm's (ashvm/src/builtins/C/raw.c).
// Addresses, syscall numbers and values are plain numbers. Nothing is checked: a wrong address crashes the
// program. Linux x86-64 only (a runtime error on Windows).
//   raw_syscall(n, a1..a6) -> the kernel's result, or negative errno
//   raw_alloc(n)           -> address of n zeroed bytes (an anonymous mmap with a 16-byte size header), 0 on failure
//   raw_free(p)            -> munmap of a raw_alloc block
//   raw_peek(p, width)     -> width 1, 2, 4 or 8 bytes at p, unsigned, little-endian
//   raw_poke(p, width, v)  -> stores the low width bytes of v at p
void codegen_builtin_raw_syscall(void);
void codegen_builtin_raw_alloc(void);
void codegen_builtin_raw_free(void);
void codegen_builtin_raw_peek(void);
void codegen_builtin_raw_poke(void);
//   raw_fill(p, v, count)  -> stores the low 32 bits of v into count consecutive dwords at p
//   raw_copy(dst, src, n)  -> copies n bytes, the ranges may overlap (memmove)
void codegen_builtin_raw_fill(void);
void codegen_builtin_raw_copy(void);

#endif
