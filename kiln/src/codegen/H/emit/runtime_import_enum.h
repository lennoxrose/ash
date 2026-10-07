#ifndef KILN_RUNTIME_IMPORT_ENUM_H
#define KILN_RUNTIME_IMPORT_ENUM_H

// The RuntimeImport enum itself, factored out of elf/H/elf_dynamic.h so
// pe/H/pe_dll_writer.h can share it too (Windows --link=shared imports the
// exact same 5 functions from libkilnrt.dll that Linux imports from
// libkilnrt.so) without two conflicting `typedef enum RuntimeImport`
// definitions in the same translation unit.
typedef enum {
#define RUNTIME_EXPORT(name, str) RUNTIME_IMPORT_##name,
#include "codegen/H/emit/runtime_exports.def"
#undef RUNTIME_EXPORT
    RUNTIME_IMPORT_COUNT_
} RuntimeImport;

#endif
