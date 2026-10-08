#ifndef KILN_ASH_GPU_IMPORT_ENUM_H
#define KILN_ASH_GPU_IMPORT_ENUM_H

// Every libashgpu.so function kiln's --link=shared codegen calls --
// single list in ash_gpu_exports.def, shared by this enum and
// elf_dynamic.c's actual table bytes, so the two can never silently
// drift out of sync (same pattern as runtime_import_enum.h).
typedef enum {
#define ASH_GPU_EXPORT(name, str) ASH_GPU_IMPORT_##name,
#include "codegen/H/emit/ash_gpu_exports.def"
#undef ASH_GPU_EXPORT
    ASH_GPU_IMPORT_COUNT_
} AshGpuImport;

#endif
