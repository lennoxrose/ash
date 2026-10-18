#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pe/H/pe_dll_writer.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/emit/bytes.h"
#include "codegen/H/strings/string_alloc.h"
#include "codegen/H/runtime/print_int.h"
#include "codegen/H/runtime/errors.h"
#include "app/H/target.h"

// Hand-written PE32+ DLL: IMAGE_FILE_DLL characteristic, an Export Data
// Directory instead of an Import one -- the Windows-side twin of
// elf/C/elf_so_writer.c. The Windows loader resolves imports-by-name via a
// BINARY SEARCH over the DLL's Name Pointer Table, which means (unlike
// ELF's hash-table symbol lookup) that table must be in sorted order, or
// GetProcAddress-style resolution silently fails to find some exports --
// see the sort below.

static const char *const export_names[] = {
#define RUNTIME_EXPORT(name, str) str,
#include "codegen/H/emit/runtime_exports.def"
#undef RUNTIME_EXPORT
};

static void put8(CodeBuf *f, uint8_t v) {
    if (f->count >= f->capacity) {
        f->capacity = f->capacity == 0 ? 256 : f->capacity * 2;
        f->code = realloc(f->code, (size_t)f->capacity);
    }
    f->code[f->count++] = v;
}
static void put16(CodeBuf *f, uint16_t v) { for (int i = 0; i < 2; i++) put8(f, (uint8_t)(v >> (8 * i))); }
static void put32(CodeBuf *f, uint32_t v) { for (int i = 0; i < 4; i++) put8(f, (uint8_t)(v >> (8 * i))); }
static void put64(CodeBuf *f, uint64_t v) { for (int i = 0; i < 8; i++) put8(f, (uint8_t)(v >> (8 * i))); }
static void put_zeros(CodeBuf *f, uint32_t n) { for (uint32_t i = 0; i < n; i++) put8(f, 0); }
static void put_str(CodeBuf *f, const char *s) { for (const char *p = s; *p; p++) put8(f, (uint8_t)*p); put8(f, 0); }
static uint32_t align_up(uint32_t v, uint32_t align) { return (v + align - 1) / align * align; }

int pe_write_shared_runtime(const char *path) {
    kiln_set_target(KILN_TARGET_WINDOWS);
    kiln_set_link_mode(KILN_LINK_SHARED);
    kiln_set_compiling_so(1);

    CodeBuf runtime;
    code_init(&runtime);
    heap_emit_alloc_routine_only(&runtime);
    bytes_emit_copy_routine_only(&runtime);
    string_alloc_emit_prefixed_routine_only(&runtime);
    print_emit_top_routine_only(&runtime);
    errors_emit_raise_routine_only(&runtime);

    int offsets[RUNTIME_IMPORT_COUNT_];
    offsets[RUNTIME_IMPORT_HEAP_ALLOC] = heap_alloc_routine_offset();
    offsets[RUNTIME_IMPORT_BYTES_COPY] = bytes_copy_routine_offset();
    offsets[RUNTIME_IMPORT_STRING_ALLOC_PREFIXED] = string_alloc_prefixed_routine_offset();
    offsets[RUNTIME_IMPORT_PRINT_TOP] = print_top_routine_offset();
    offsets[RUNTIME_IMPORT_RAISE] = errors_raise_routine_offset();

    kiln_set_compiling_so(0);

    // Sort export indices by name (lexicographic byte compare, matching
    // the Windows loader's own binary search over the Name Pointer Table).
    int sorted[RUNTIME_IMPORT_COUNT_];
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) sorted[i] = i;
    for (int i = 1; i < RUNTIME_IMPORT_COUNT_; i++) {
        int key = sorted[i], j = i - 1;
        while (j >= 0 && strcmp(export_names[sorted[j]], export_names[key]) > 0) { sorted[j + 1] = sorted[j]; j--; }
        sorted[j + 1] = key;
    }

    uint32_t code_rva = KILN_PE_DLL_CODE_START_OFFSET;
    uint32_t name_off[RUNTIME_IMPORT_COUNT_]; // RVA of each export's name string, indexed by ORIGINAL (enum) order
    uint32_t cursor = code_rva + (uint32_t)runtime.count;
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) { name_off[i] = cursor; cursor += (uint32_t)strlen(export_names[i]) + 1; }
    uint32_t dll_name_rva = cursor; cursor += (uint32_t)strlen(KILN_LIBKILNRT_DLL_NAME) + 1;

    uint32_t section_virtual_size = cursor - KILN_PE_DLL_SECTION_RVA;
    uint32_t section_raw_size = align_up(section_virtual_size, KILN_PE_FILE_ALIGN);
    uint32_t image_size = align_up(KILN_PE_DLL_SECTION_RVA + section_virtual_size, KILN_PE_SECTION_ALIGN);

    CodeBuf f;
    code_init(&f);

    put8(&f, 'M'); put8(&f, 'Z');
    put_zeros(&f, 58);
    put32(&f, 64);

    put8(&f, 'P'); put8(&f, 'E'); put8(&f, 0); put8(&f, 0);
    put16(&f, 0x8664);
    put16(&f, 1); // NumberOfSections
    put32(&f, 0); put32(&f, 0); put32(&f, 0);
    put16(&f, 240);
    put16(&f, 0x0002 | 0x2000); // EXECUTABLE_IMAGE | DLL

    put16(&f, 0x20B);
    put8(&f, 14); put8(&f, 0);
    put32(&f, section_raw_size);
    put32(&f, 0); put32(&f, 0);
    put32(&f, 0);                       // AddressOfEntryPoint = 0 (no DllMain, nothing to run at load/unload)
    put32(&f, KILN_PE_DLL_SECTION_RVA); // BaseOfCode
    put64(&f, KILN_PE_DLL_IMAGE_BASE);
    put32(&f, KILN_PE_SECTION_ALIGN);
    put32(&f, KILN_PE_FILE_ALIGN);
    put16(&f, 6); put16(&f, 0);
    put16(&f, 0); put16(&f, 0);
    put16(&f, 6); put16(&f, 0);
    put32(&f, 0);
    put32(&f, image_size);
    put32(&f, KILN_PE_HEADERS_SIZE);
    put32(&f, 0);
    put16(&f, 3); // Subsystem = WINDOWS_CUI (inherited from the loading process; irrelevant for a DLL but harmless)
    put16(&f, 0); // DllCharacteristics -- no DYNAMIC_BASE, fixed base like everything else kiln emits
    put64(&f, 0x100000); put64(&f, 0x1000);
    put64(&f, 0x100000); put64(&f, 0x1000);
    put32(&f, 0);
    put32(&f, 16);
    for (int i = 0; i < 16; i++) {
        if (i == 0) { put32(&f, KILN_PE_DLL_EXPORT_DIR_ADDR - KILN_PE_DLL_IMAGE_BASE); put32(&f, KILN_PE_DLL_EXPORT_DIR_SIZE); } // Export Table
        else { put32(&f, 0); put32(&f, 0); }
    }

    const char *sec_name = ".text";
    for (int i = 0; i < 8; i++) put8(&f, (uint8_t)(i < (int)strlen(sec_name) ? sec_name[i] : 0));
    put32(&f, section_virtual_size);
    put32(&f, KILN_PE_DLL_SECTION_RVA);
    put32(&f, section_raw_size);
    put32(&f, KILN_PE_HEADERS_SIZE);
    put32(&f, 0); put32(&f, 0);
    put16(&f, 0); put16(&f, 0);
    put32(&f, 0x20u | 0x20000000u | 0x40000000u | 0x80000000u);

    while (f.count < (int)KILN_PE_HEADERS_SIZE) put8(&f, 0);

    // IMAGE_EXPORT_DIRECTORY
    put32(&f, 0);                                              // Characteristics
    put32(&f, 0);                                              // TimeDateStamp
    put16(&f, 0); put16(&f, 0);                                // Major/MinorVersion
    put32(&f, dll_name_rva);                                   // Name
    put32(&f, 1);                                               // Base (ordinals start at 1)
    put32(&f, (uint32_t)RUNTIME_IMPORT_COUNT_);                 // NumberOfFunctions
    put32(&f, (uint32_t)RUNTIME_IMPORT_COUNT_);                 // NumberOfNames
    put32(&f, KILN_PE_DLL_FUNC_ARRAY_ADDR - KILN_PE_DLL_IMAGE_BASE);
    put32(&f, KILN_PE_DLL_NAME_ARRAY_ADDR - KILN_PE_DLL_IMAGE_BASE);
    put32(&f, KILN_PE_DLL_ORDINAL_ARRAY_ADDR - KILN_PE_DLL_IMAGE_BASE);

    // AddressOfFunctions: natural (enum) order, RVA of each routine's entry point
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) put32(&f, code_rva + (uint32_t)offsets[i]);
    // AddressOfNames: SORTED order (binary search requirement)
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) put32(&f, name_off[sorted[i]]);
    // AddressOfNameOrdinals: for each sorted name, the (0-based) index into AddressOfFunctions
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) put16(&f, (uint16_t)sorted[i]);

    for (int i = 0; i < runtime.count; i++) put8(&f, runtime.code[i]);
    free(runtime.code);

    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) put_str(&f, export_names[i]);
    put_str(&f, KILN_LIBKILNRT_DLL_NAME);

    while (f.count < (int)(KILN_PE_HEADERS_SIZE + section_raw_size)) put8(&f, 0);

    FILE *out = fopen(path, "wb");
    if (!out) { fprintf(stderr, "kiln: could not open '%s' for writing\n", path); free(f.code); return 1; }
    size_t written = fwrite(f.code, 1, (size_t)f.count, out);
    fclose(out);
    free(f.code);
    if (written != (size_t)f.count) { fprintf(stderr, "kiln: short write to '%s'\n", path); return 1; }
    return 0;
}
