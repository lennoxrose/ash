#ifndef KILN_PE_DLL_WRITER_H
#define KILN_PE_DLL_WRITER_H
#include "codegen/H/emit/runtime_import_enum.h"
#include "pe/H/pe_writer.h"

// libkilnrt.dll: same fixed-base, no-ASLR, no-relocations philosophy as
// every other kiln output (a real DLL would normally get a .reloc
// section so the loader can rebase it if its preferred address is
// taken; kiln skips that the same way it skips PIC/ASLR support for its
// own executables -- consistent, not a shortcut specific to this file).
// Distinct base from KILN_PE_IMAGE_BASE (the executable's own) since
// both could conceivably be loaded into the same process.
#define KILN_PE_DLL_IMAGE_BASE 0x180000000ULL
#define KILN_PE_DLL_SECTION_RVA KILN_PE_SECTION_ALIGN

// IMAGE_EXPORT_DIRECTORY (40 bytes) + 3 fixed-size arrays (sized only by
// RUNTIME_IMPORT_COUNT_, never by name length) -- code starts right after,
// so its address is a compile-time constant kiln_code_base() can use
// (see codegen/C/emit/layout.c); the variable-length name strings go in the
// tail after code, same trick as pe_writer.h's hint/name table.
#define KILN_PE_DLL_EXPORT_DIR_ADDR (KILN_PE_DLL_IMAGE_BASE + KILN_PE_DLL_SECTION_RVA)
#define KILN_PE_DLL_EXPORT_DIR_SIZE 40u
#define KILN_PE_DLL_FUNC_ARRAY_ADDR (KILN_PE_DLL_EXPORT_DIR_ADDR + KILN_PE_DLL_EXPORT_DIR_SIZE)
#define KILN_PE_DLL_FUNC_ARRAY_SIZE (4u * (unsigned)RUNTIME_IMPORT_COUNT_)
#define KILN_PE_DLL_NAME_ARRAY_ADDR (KILN_PE_DLL_FUNC_ARRAY_ADDR + KILN_PE_DLL_FUNC_ARRAY_SIZE)
#define KILN_PE_DLL_NAME_ARRAY_SIZE (4u * (unsigned)RUNTIME_IMPORT_COUNT_)
#define KILN_PE_DLL_ORDINAL_ARRAY_ADDR (KILN_PE_DLL_NAME_ARRAY_ADDR + KILN_PE_DLL_NAME_ARRAY_SIZE)
#define KILN_PE_DLL_ORDINAL_ARRAY_SIZE (2u * (unsigned)RUNTIME_IMPORT_COUNT_)

#define KILN_PE_DLL_CODE_START_OFFSET \
    (KILN_PE_DLL_SECTION_RVA + (KILN_PE_DLL_EXPORT_DIR_SIZE + KILN_PE_DLL_FUNC_ARRAY_SIZE + \
     KILN_PE_DLL_NAME_ARRAY_SIZE + KILN_PE_DLL_ORDINAL_ARRAY_SIZE))

// Compiles the runtime (same 5 routines as elf_so_writer.h) into a fresh
// CodeBuf and writes it as a PE32+ DLL exporting them by name, loadable
// by the real Windows PE loader. Returns 0 on success.
int pe_write_shared_runtime(const char *path);

#endif
