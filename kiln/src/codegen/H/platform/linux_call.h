#ifndef KILN_LINUX_CALL_H
#define KILN_LINUX_CALL_H
#include "codegen/H/emit/emit.h"
#include "elf/H/elf_dynamic_call.h"

// SysV x86-64 calling convention wrapper for calling into a SECOND
// needed library's real, normally-compiled function (libashgpu.so --
// see elf/H/elf_dynamic.h) -- new territory the same way win_call.h was
// for libkilnrt.dll's imports, but for a different reason: kiln's OWN
// runtime routines (heap_alloc, print_top, ...), the only things
// elf_dynamic_call was ever used for before this, are THEMSELVES
// kiln's own hand-rolled machine code, which never uses an
// alignment-sensitive instruction (emit_sse.h's movsd/movq never need
// it) -- so kiln's codegen has never had to track RSP's 16-byte
// alignment at a CALL anywhere, the same gap win_call.h's own header
// comment describes for Windows. ash_gpu_matrix_multiply is real
// gcc/g++-compiled code (SSE/AVX doubles, OpenMP), which DOES rely on
// that alignment -- calling it misaligned risks a `movaps`-class fault,
// not a wrong answer.
//
// Same technique as win_call_begin/end (dynamic `and rsp, -16` plus a
// saved-RSP scratch slot), reusing that exact slot
// (win_call_scratch_offset(), parser/H/declarations/vars.h) rather than
// reserving a second one: a single kiln target is always Linux or
// Windows, never both, so the slot is never needed for its Windows
// purpose and this purpose in the same compiled program. No shadow
// space here (SysV has none) -- that's the only difference from
// win_call_begin.
// Neither of these touches RDI/RSI/RDX/RCX -- only RSP and the scratch
// slot -- so the caller loads the callee's C-ABI arguments into those
// registers in whatever order is convenient, either side of
// linux_abi_call_begin(), then calls ash_gpu_dynamic_call() before
// linux_abi_call_end(). See codegen/C/collections/matrix_mul.c for the
// one caller so far.
void linux_abi_call_begin(CodeBuf *code);
void linux_abi_call_end(CodeBuf *code);

#endif
