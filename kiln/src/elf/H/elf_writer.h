#ifndef KILN_ELF_WRITER_H
#define KILN_ELF_WRITER_H
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/runtime_layout.h"

// Layout constants shared with codegen/C/runtime/heap.c: kiln generates a fixed,
// non-PIE executable (always loaded at KILN_LOAD_BASE), so absolute
// addresses are known at compile time. Milestone 5 needs one such fixed
// address for the heap bump pointer (a "global" -- there's no per-call
// frame that's reachable from every function, since each call gets its
// own fresh RBP), reserved as a small region between the program headers
// and the actual code.
#define KILN_LOAD_BASE 0x400000ULL // page-aligned, traditional non-PIE x86-64 base
#define KILN_EHDR_SIZE 64
#define KILN_PHDR_SIZE 56

// Milestone 9: the globals region also holds kiln's own hand-rolled
// setjmp/longjmp equivalent for attempt/handle (see codegen/C/runtime/errors.c's raise
// routine and parser/C/statements/attempt_handle.c) -- a fixed-depth stack of "handler"
// records, since this is a freestanding binary with no libc setjmp to
// call. Each handler is {saved_rsp, saved_rbp, target_addr,
// error_slot_rbp_offset}, 4 x int64. Bounded the same way every other
// table in this project is (MAX_KILN_VARS, MAX_KILN_FUNCS, ...) --
// exceeding it is a hard (uncatchable) fatal error, not undefined
// behavior, since overflowing past it would silently corrupt the
// adjacent code segment.
// Milestone: argv(). The kernel hands a freshly-started process its
// stack laid out as [argc, argv[0], argv[1], ..., NULL, envp...], with
// RSP pointing at argc, before any of THIS program's own code runs.
// compile_program's very first instruction (before even `mov rbp,rsp`)
// saves that original RSP here -- a fixed absolute address, reachable
// from any frame later on, since a called function's own RBP is no
// longer the original one by the time argv() might be called from
// inside it (see codegen/C/io/argv_builtin.c).
#define KILN_ARGV_SIZE 8

#define KILN_GLOBALS_SIZE (8 + KILN_TRY_DEPTH_SIZE + KILN_TRY_HANDLERS_SIZE + KILN_ARGV_SIZE + KILN_MODSTATE_SIZE) // heap_ptr, try_depth, handler stack, argv, module state
#define KILN_GLOBALS_ADDR (KILN_LOAD_BASE + KILN_EHDR_SIZE + KILN_PHDR_SIZE)
#define KILN_TRY_DEPTH_ADDR (KILN_GLOBALS_ADDR + 8)
#define KILN_TRY_HANDLERS_ADDR (KILN_TRY_DEPTH_ADDR + KILN_TRY_DEPTH_SIZE)
#define KILN_ARGV_ADDR (KILN_TRY_HANDLERS_ADDR + KILN_TRY_HANDLERS_SIZE)
#define KILN_MODSTATE_ADDR (KILN_ARGV_ADDR + KILN_ARGV_SIZE)
#define KILN_CODE_START_OFFSET (KILN_EHDR_SIZE + KILN_PHDR_SIZE + KILN_GLOBALS_SIZE)

// Writes `machine_code` as a minimal, standalone, directly-executable
// Linux x86-64 ELF64 binary at `path` and marks it executable (chmod
// +x). No section headers at all -- the kernel loader only reads the ELF
// header and program headers to run a binary ("tiny ELF" technique).
// Returns 0 on success, prints an error and returns nonzero on failure.
int elf_write_executable(const char *path, const CodeBuf *machine_code);

#endif
