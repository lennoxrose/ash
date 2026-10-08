#ifndef KILN_PE_WRITER_H
#define KILN_PE_WRITER_H
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/runtime_layout.h"
#include "codegen/H/emit/runtime_import_enum.h"

// Kiln generates a fixed, non-relocated PE32+ executable, same "always
// loaded at one known address" philosophy as elf/H/elf_writer.h's
// KILN_LOAD_BASE -- absolute addresses for string literals, closure code
// addresses, and (this file's addition) imported-function IAT slots are
// all known at compile time, no relocation table needed. 0x140000000 is
// the address range modern PE tooling (MSVC/link.exe) itself defaults 64
// bit executables to, so it's a safe, unsurprising choice for loaders.
#define KILN_PE_IMAGE_BASE 0x140000000ULL
#define KILN_PE_SECTION_ALIGN 0x1000u
#define KILN_PE_FILE_ALIGN 0x200u

// DOS header (64) + "PE\0\0" (4) + COFF header (20) + PE32+ optional
// header (112 fixed fields + 16 data directories * 8 bytes = 240) +
// one section header (40) = 368, rounded up to KILN_PE_FILE_ALIGN.
#define KILN_PE_HEADERS_RAW_SIZE 368u
#define KILN_PE_HEADERS_SIZE 0x200u

// The one-and-only section: globals region, then import machinery, then
// code, then (past the end of `code`, so its size doesn't affect any of
// the fixed offsets below) the variable-length hint/name + DLL-name
// tail -- see pe_writer.c's header comment for why that ordering matters.
#define KILN_PE_SECTION_RVA KILN_PE_SECTION_ALIGN

#define KILN_PE_GLOBALS_ADDR (KILN_PE_IMAGE_BASE + KILN_PE_SECTION_RVA)
#define KILN_PE_HEAP_PTR_ADDR (KILN_PE_GLOBALS_ADDR + 0)
#define KILN_PE_TRY_DEPTH_ADDR (KILN_PE_GLOBALS_ADDR + 8)
#define KILN_PE_TRY_HANDLERS_ADDR (KILN_PE_TRY_DEPTH_ADDR + KILN_TRY_DEPTH_SIZE)
// No argv slot: GetCommandLineA() can be called at any point, unlike
// Linux's argv() which has to snapshot the kernel's original RSP before
// anything else touches it (see codegen/C/io/argv_builtin.c).
#define KILN_PE_MODSTATE_ADDR (KILN_PE_TRY_HANDLERS_ADDR + KILN_TRY_HANDLERS_SIZE)
#define KILN_PE_GLOBALS_SIZE (8 + KILN_TRY_DEPTH_SIZE + KILN_TRY_HANDLERS_SIZE + KILN_MODSTATE_SIZE)

// Every kernel32.dll function kiln's Windows codegen calls -- single list
// in pe_imports.def, shared by this enum and pe_writer.c's actual table
// bytes, so the two can never silently drift out of sync.
typedef enum {
#define PE_IMPORT(name, str) PE_IMPORT_##name,
#include "pe/H/pe_imports.def"
#undef PE_IMPORT
    PE_IMPORT_COUNT_
} PeImport;

// Import Directory Table: one IMAGE_IMPORT_DESCRIPTOR (20 bytes) for
// kernel32.dll, plus one zeroed terminator entry.
#define KILN_PE_IMPORT_DIR_ADDR (KILN_PE_GLOBALS_ADDR + KILN_PE_GLOBALS_SIZE)
#define KILN_PE_IMPORT_DIR_SIZE 40u

// Import Lookup Table and Import Address Table: one 8-byte entry per
// import plus a zero terminator each. Fixed size regardless of function
// NAME lengths (those live in the variable-length tail written after
// `code`), which is exactly what lets codegen address IAT slots as
// compile-time constants without knowing the final code length first.
#define KILN_PE_ILT_ADDR (KILN_PE_IMPORT_DIR_ADDR + KILN_PE_IMPORT_DIR_SIZE)
#define KILN_PE_ILT_SIZE (8u * ((unsigned)PE_IMPORT_COUNT_ + 1u))
#define KILN_PE_IAT_ADDR (KILN_PE_ILT_ADDR + KILN_PE_ILT_SIZE)
#define KILN_PE_IAT_SIZE (8u * ((unsigned)PE_IMPORT_COUNT_ + 1u))

#define KILN_PE_CODE_START_OFFSET \
    (KILN_PE_SECTION_RVA + (KILN_PE_GLOBALS_SIZE + KILN_PE_IMPORT_DIR_SIZE + KILN_PE_ILT_SIZE + KILN_PE_IAT_SIZE))

// Absolute VA of `which`'s IAT slot -- what the loader overwrites with
// the resolved function pointer at load time. codegen/C/platform/win_call.c loads
// this address, dereferences it, and calls through the result (see that
// file for why no RIP-relative addressing is needed here).
uint64_t pe_import_addr(PeImport which);

// ---- --link=shared layout: a SECOND imported DLL (libkilnrt.dll,
// see pe/H/pe_dll_writer.h) alongside kernel32.dll. Kept as a fully
// parallel set of constants, never touched by the static-mode ones
// above, so --link=static output stays byte-for-byte unchanged --
// pe_writer.c picks one whole layout or the other based on link mode,
// never mixes them. ----
#define KILN_LIBKILNRT_DLL_NAME "libkilnrt.dll"

// 3 descriptors (kernel32.dll, libkilnrt.dll, null terminator) instead
// of 2 (kernel32.dll, null). Both ILTs are grouped before both IATs
// (rather than interleaved per-DLL) so the combined IAT region is
// CONTIGUOUS -- the optional header's IAT data directory entry is a
// single {RVA,Size} pair, which can only correctly describe one
// contiguous range.
#define KILN_PE_SHARED_IMPORT_DIR_SIZE 60u
#define KILN_PE_SHARED_ILT_K32_ADDR (KILN_PE_IMPORT_DIR_ADDR + KILN_PE_SHARED_IMPORT_DIR_SIZE)
#define KILN_PE_SHARED_ILT_K32_SIZE (8u * ((unsigned)PE_IMPORT_COUNT_ + 1u))
#define KILN_PE_SHARED_ILT_RT_ADDR (KILN_PE_SHARED_ILT_K32_ADDR + KILN_PE_SHARED_ILT_K32_SIZE)
#define KILN_PE_SHARED_ILT_RT_SIZE (8u * ((unsigned)RUNTIME_IMPORT_COUNT_ + 1u))
#define KILN_PE_SHARED_IAT_K32_ADDR (KILN_PE_SHARED_ILT_RT_ADDR + KILN_PE_SHARED_ILT_RT_SIZE)
#define KILN_PE_SHARED_IAT_K32_SIZE (8u * ((unsigned)PE_IMPORT_COUNT_ + 1u))
#define KILN_PE_SHARED_IAT_RT_ADDR (KILN_PE_SHARED_IAT_K32_ADDR + KILN_PE_SHARED_IAT_K32_SIZE)
#define KILN_PE_SHARED_IAT_RT_SIZE (8u * ((unsigned)RUNTIME_IMPORT_COUNT_ + 1u))

#define KILN_PE_SHARED_CODE_START_OFFSET \
    (KILN_PE_SECTION_RVA + (KILN_PE_GLOBALS_SIZE + KILN_PE_SHARED_IMPORT_DIR_SIZE + \
     KILN_PE_SHARED_ILT_K32_SIZE + KILN_PE_SHARED_IAT_K32_SIZE + \
     KILN_PE_SHARED_ILT_RT_SIZE + KILN_PE_SHARED_IAT_RT_SIZE))

// Absolute VA of `which`'s libkilnrt.dll IAT slot (shared-mode layout).
uint64_t pe_runtime_import_addr(RuntimeImport which);

// Writes `machine_code` as a PE32+ console executable that imports from
// BOTH kernel32.dll and libkilnrt.dll (the shared-mode layout above).
int pe_write_dynamic_executable(const char *path, const CodeBuf *machine_code);

// Writes `machine_code` as a minimal, standalone PE32+ console executable
// at `path` (hand-written headers/import tables, same "no external
// linker" spirit as elf_write_executable). Returns 0 on success.
int pe_write_executable(const char *path, const CodeBuf *machine_code);

#endif
