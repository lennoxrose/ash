#ifndef KILN_ELF_DYNAMIC_H
#define KILN_ELF_DYNAMIC_H
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/runtime_import_enum.h"
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
#define KILN_RPATH "/usr/local/lib/kiln"
#define KILN_LIBKILNRT_SONAME "libkilnrt.so"
#define KILN_INTERP_STR "/lib64/ld-linux-x86-64.so.2"

// nsyms includes the mandatory null 0th symbol.
#define KILN_DYN_NSYMS ((unsigned)RUNTIME_IMPORT_COUNT_ + 1u)
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
#define KILN_DYN_GOT_SIZE (8u * (unsigned)RUNTIME_IMPORT_COUNT_)

#define KILN_DYN_ENTRY_SIZE 16u // Elf64_Dyn: {Elf64_Sxword d_tag; union d_val/d_ptr;}
#define KILN_DYN_TABLE_NENT_TOTAL 10u // NEEDED,RPATH,HASH,STRTAB,SYMTAB,STRSZ,SYMENT,RELA,RELASZ,RELAENT,NULL(11, see .c)
#define KILN_DYN_TABLE_ADDR (KILN_DYN_GOT_ADDR + KILN_DYN_GOT_SIZE)
#define KILN_DYN_TABLE_SIZE ((KILN_DYN_TABLE_NENT_TOTAL + 1u) * KILN_DYN_ENTRY_SIZE) // +1 for the DT_NULL terminator

#define KILN_DYN_RELAENT_SIZE 24u // Elf64_Rela: {Elf64_Addr; Elf64_Xword info; Elf64_Sxword addend}
#define KILN_DYN_RELA_ADDR (KILN_DYN_TABLE_ADDR + KILN_DYN_TABLE_SIZE)
#define KILN_DYN_RELA_SIZE (KILN_DYN_RELAENT_SIZE * (unsigned)RUNTIME_IMPORT_COUNT_)

#define KILN_DYN_GLOBALS_ADDR (KILN_DYN_RELA_ADDR + KILN_DYN_RELA_SIZE)
#define KILN_DYN_GLOBALS_SIZE (8 + KILN_TRY_DEPTH_SIZE + KILN_TRY_HANDLERS_SIZE + KILN_ARGV_SIZE)
#define KILN_DYN_CODE_START_OFFSET (KILN_DYN_GLOBALS_ADDR + KILN_DYN_GLOBALS_SIZE - KILN_LOAD_BASE)

// Absolute VA of `which`'s GOT slot -- loaded via a compile-time-known
// immediate (kiln's executables are always fixed-base, same reasoning as
// pe_import_addr) -- deref it and call through the result.
uint64_t elf_dynamic_got_addr(RuntimeImport which);

// Writes `machine_code` as a dynamically-linked ELF64 executable at
// `path`, expecting libkilnrt.so to be installed at KILN_RPATH by the
// time this program is run (see elf_so_writer.h). Returns 0 on success.
int elf_write_dynamic_executable(const char *path, const CodeBuf *machine_code);

#endif
