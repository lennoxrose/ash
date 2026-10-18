#ifndef KILN_ELF_DYNAMIC_H
#define KILN_ELF_DYNAMIC_H
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/runtime_import_enum.h"
#include "codegen/H/emit/ash_gpu_import_enum.h"
#include "elf/H/elf_writer.h"

// Layout for a --link=shared kiln executable: real dynamic linking, the
// kernel hands off to the system's actual /lib64/ld-linux-x86-64.so.2 (a
// PT_INTERP segment says so), which reads PT_DYNAMIC, finds
// libkilnrt.so via DT_NEEDED+DT_RPATH, and resolves each imported
// runtime function into a GOT slot this file also lays out -- see
// elf_dynamic.c's header comment for the full section-by-section layout
// and elf_so_writer.h for the library side of the same relationship.
//
// Design validated against the REAL system dynamic linker with a
// hand-written prototype before any of this was ported into kiln itself
// (two hand-built ELF files, one ET_DYN "library" and one ET_EXEC
// "executable" with PT_INTERP/PT_DYNAMIC/GOT/RELA, loaded and resolved
// correctly by /lib64/ld-linux-x86-64.so.2) -- this header encodes that
// same validated structure, not a first attempt.
// Colon-separated search list. kiln/Makefile overrides it to also name the
// repo's bin/extensions/pyre (where Pyre builds libpyre.so) ahead of the
// system install dir.
#ifndef KILN_RPATH
#define KILN_RPATH "/usr/local/lib/kiln"
#endif
#define KILN_LIBKILNRT_SONAME "libkilnrt.so"
// ideas/assigned.md: a SECOND needed library, Pyre's compute runtime --
// kept as a parallel DT_NEEDED next to libkilnrt.so's, never merged into
// it (libkilnrt.so is kiln's own hand-rolled routines; libpyre.so is a
// normal gcc/g++-built shared object this project doesn't build). ld.so
// resolves each imported symbol name against ALL needed libraries, so
// the two don't need separate hash/dynsym/GOT regions -- one combined
// table (KILN_DYN_TOTAL_IMPORTS below) covering both libraries' symbols
// is correct and the much smaller change.
#define KILN_ASHGPU_SONAME "libpyre.so"
#define KILN_INTERP_STR "/lib64/ld-linux-x86-64.so.2"

// Every symbol this executable needs resolved, from EITHER needed
// library -- kiln's own (RUNTIME_IMPORT_COUNT_, from libkilnrt.so) then
// Pyre's (ASH_GPU_IMPORT_COUNT_, from libpyre.so), in that order.
#define KILN_DYN_TOTAL_IMPORTS ((unsigned)RUNTIME_IMPORT_COUNT_ + (unsigned)ASH_GPU_IMPORT_COUNT_)

// nsyms includes the mandatory null 0th symbol.
#define KILN_DYN_NSYMS (KILN_DYN_TOTAL_IMPORTS + 1u)
#define KILN_DYN_NBUCKET KILN_DYN_NSYMS

// Section order: everything whose address codegen (or another fixed-size
// section) needs as a compile-time constant comes first, in FIXED-SIZE
// pieces only (sizes depending on RUNTIME_IMPORT_COUNT_, never on string
// lengths); .dynstr -- the one variable-length piece -- goes after the
// code instead, exactly like pe_writer.h's hint/name tail, computed at
// write time in elf_dynamic.c once the final code length is known.
//   [ehdr][phdrs][interp][hash][dynsym][GOT][dynamic][rela][globals][code][dynstr]
#define KILN_DYN_INTERP_ADDR (KILN_LOAD_BASE + KILN_EHDR_SIZE + 3u * KILN_PHDR_SIZE)
#define KILN_DYN_INTERP_SIZE (sizeof(KILN_INTERP_STR)) // includes NUL

#define KILN_DYN_HASH_ADDR (KILN_DYN_INTERP_ADDR + KILN_DYN_INTERP_SIZE)
#define KILN_DYN_HASH_SIZE (8u + 4u * KILN_DYN_NBUCKET + 4u * KILN_DYN_NSYMS)

#define KILN_DYN_SYMENT_SIZE 24u
#define KILN_DYN_SYMTAB_ADDR (KILN_DYN_HASH_ADDR + KILN_DYN_HASH_SIZE)
#define KILN_DYN_SYMTAB_SIZE (KILN_DYN_NSYMS * KILN_DYN_SYMENT_SIZE)

// GOT: one 8-byte slot per imported runtime function, zeroed at rest,
// filled in by ld.so with the resolved absolute address before this
// program's own entry point ever runs. Fixed size/offset regardless of
// symbol NAME lengths, exactly like pe_writer.h's IAT -- codegen can
// address a slot as a compile-time constant.
#define KILN_DYN_GOT_ADDR (KILN_DYN_SYMTAB_ADDR + KILN_DYN_SYMTAB_SIZE)
#define KILN_DYN_GOT_SIZE (8u * KILN_DYN_TOTAL_IMPORTS)

#define KILN_DYN_ENTRY_SIZE 16u // Elf64_Dyn: {Elf64_Sxword d_tag; union d_val/d_ptr;}
// NEEDED(libkilnrt.so),NEEDED(libpyre.so),RPATH,HASH,STRTAB,SYMTAB,STRSZ,SYMENT,RELA,RELASZ,RELAENT,NULL(12, see .c)
#define KILN_DYN_TABLE_NENT_TOTAL 11u
#define KILN_DYN_TABLE_ADDR (KILN_DYN_GOT_ADDR + KILN_DYN_GOT_SIZE)
#define KILN_DYN_TABLE_SIZE ((KILN_DYN_TABLE_NENT_TOTAL + 1u) * KILN_DYN_ENTRY_SIZE) // +1 for the DT_NULL terminator

#define KILN_DYN_RELAENT_SIZE 24u // Elf64_Rela: {Elf64_Addr; Elf64_Xword info; Elf64_Sxword addend}
#define KILN_DYN_RELA_ADDR (KILN_DYN_TABLE_ADDR + KILN_DYN_TABLE_SIZE)
#define KILN_DYN_RELA_SIZE (KILN_DYN_RELAENT_SIZE * KILN_DYN_TOTAL_IMPORTS)

#define KILN_DYN_GLOBALS_ADDR (KILN_DYN_RELA_ADDR + KILN_DYN_RELA_SIZE)
#define KILN_DYN_GLOBALS_SIZE (8 + KILN_HEAP_LIMIT_SIZE + KILN_TRY_DEPTH_SIZE + KILN_TRY_HANDLERS_SIZE + KILN_ARGV_SIZE + KILN_MODSTATE_SIZE)
#define KILN_DYN_MODSTATE_ADDR (KILN_DYN_GLOBALS_ADDR + 8 + KILN_HEAP_LIMIT_SIZE + KILN_TRY_DEPTH_SIZE + KILN_TRY_HANDLERS_SIZE + KILN_ARGV_SIZE)
#define KILN_DYN_CODE_START_OFFSET (KILN_DYN_GLOBALS_ADDR + KILN_DYN_GLOBALS_SIZE - KILN_LOAD_BASE)

// Absolute VA of `which`'s GOT slot -- loaded via a compile-time-known
// immediate (kiln's executables are always fixed-base, same reasoning as
// pe_import_addr) -- deref it and call through the result.
uint64_t elf_dynamic_got_addr(RuntimeImport which);

// Same idea, for the second needed library's imports -- their GOT slots
// sit right after kiln's own RUNTIME_IMPORT_COUNT_ slots in the same
// combined GOT region (KILN_DYN_TOTAL_IMPORTS above).
uint64_t ash_gpu_dynamic_got_addr(AshGpuImport which);

// Writes `machine_code` as a dynamically-linked ELF64 executable at
// `path`, expecting libkilnrt.so to be installed at KILN_RPATH by the
// time this program is run (see elf_so_writer.h). Returns 0 on success.
int elf_write_dynamic_executable(const char *path, const CodeBuf *machine_code);

#endif
