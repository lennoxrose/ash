#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pe/pe_writer.h"
#include "target.h"

// Hand-written minimal PE32+ console executable -- the Windows-side twin
// of elf/elf_writer.c's "tiny ELF" technique: one RWX section holding
// everything (globals, import tables, code), a fixed non-relocated image
// base, no external linker involved.
//
// Section layout (see pe_writer.h for the address math):
//   [globals][import directory][ILT][IAT][code][hint/name entries][dll name]
// Globals/import-dir/ILT/IAT all have sizes fixed by pe_imports.def alone
// (never by name string lengths), so codegen can address IAT slots and
// the code's own start address as compile-time constants -- the only
// things whose size depends on `machine_code->count` or import name
// lengths (the hint/name table, the dll name string) are placed AFTER
// code, computed right here at file-write time once that length is known.

static const char *const pe_import_names[] = {
#define PE_IMPORT(name, str) str,
#include "pe/pe_imports.def"
#undef PE_IMPORT
};

// --link=shared moves kernel32.dll's IAT to a different offset (there's
// a second DLL's import machinery ahead of it -- see pe_writer.h) --
// this has to pick the layout actually in use or it silently points at
// the wrong slot, which is exactly the kind of bug that surfaces as a
// jump to a garbage address rather than a clean error (caught by testing
// the Windows shared build under Wine, not assumed correct from the
// static-mode case alone).
uint64_t pe_import_addr(PeImport which) {
    if (kiln_get_link_mode() == KILN_LINK_SHARED) return KILN_PE_SHARED_IAT_K32_ADDR + 8ULL * (uint64_t)which;
    return KILN_PE_IAT_ADDR + 8ULL * (uint64_t)which;
}

uint64_t pe_runtime_import_addr(RuntimeImport which) {
    return KILN_PE_SHARED_IAT_RT_ADDR + 8ULL * (uint64_t)which;
}

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

// 2-byte hint + null-terminated name, padded to an even total length (PE
// spec requires each Hint/Name entry to start on a 2-byte boundary).
static uint32_t hint_name_entry_size(const char *name) {
    uint32_t n = 2 + (uint32_t)strlen(name) + 1;
    return (n % 2 == 0) ? n : n + 1;
}

int pe_write_executable(const char *path, const CodeBuf *machine_code) {
    // ---- compute the variable-length tail (hint/name table + DLL name),
    // which only PE_writer itself needs to know the layout of ----
    uint32_t code_rva = KILN_PE_CODE_START_OFFSET;
    uint32_t hint_name_rva = code_rva + (uint32_t)machine_code->count;
    uint32_t hint_name_offsets[PE_IMPORT_COUNT_];
    uint32_t cursor = hint_name_rva;
    for (int i = 0; i < PE_IMPORT_COUNT_; i++) {
        hint_name_offsets[i] = cursor;
        cursor += hint_name_entry_size(pe_import_names[i]);
    }
    uint32_t dll_name_rva = cursor;
    uint32_t section_virtual_size = dll_name_rva + (uint32_t)strlen("kernel32.dll") + 1 - KILN_PE_SECTION_RVA;
    uint32_t section_raw_size = align_up(section_virtual_size, KILN_PE_FILE_ALIGN);
    uint32_t image_size = align_up(KILN_PE_SECTION_RVA + section_virtual_size, KILN_PE_SECTION_ALIGN);

    CodeBuf f;
    code_init(&f);

    // ---- IMAGE_DOS_HEADER (64 bytes, e_lfanew points right past it) ----
    put8(&f, 'M'); put8(&f, 'Z');
    put_zeros(&f, 58);
    put32(&f, 64); // e_lfanew

    // ---- PE signature + IMAGE_FILE_HEADER (COFF) ----
    put8(&f, 'P'); put8(&f, 'E'); put8(&f, 0); put8(&f, 0);
    put16(&f, 0x8664); // Machine = AMD64
    put16(&f, 1);      // NumberOfSections
    put32(&f, 0);      // TimeDateStamp
    put32(&f, 0);      // PointerToSymbolTable
    put32(&f, 0);      // NumberOfSymbols
    put16(&f, 240);    // SizeOfOptionalHeader
    put16(&f, 0x0002 | 0x0020); // EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE

    // ---- IMAGE_OPTIONAL_HEADER64 ----
    put16(&f, 0x20B); // Magic = PE32+
    put8(&f, 14); put8(&f, 0); // Linker version
    put32(&f, section_raw_size); // SizeOfCode
    put32(&f, 0);                // SizeOfInitializedData
    put32(&f, 0);                // SizeOfUninitializedData
    put32(&f, code_rva);         // AddressOfEntryPoint
    put32(&f, KILN_PE_SECTION_RVA); // BaseOfCode
    put64(&f, KILN_PE_IMAGE_BASE);
    put32(&f, KILN_PE_SECTION_ALIGN);
    put32(&f, KILN_PE_FILE_ALIGN);
    put16(&f, 6); put16(&f, 0); // OS version
    put16(&f, 0); put16(&f, 0); // Image version
    put16(&f, 6); put16(&f, 0); // Subsystem version
    put32(&f, 0);                // Win32VersionValue
    put32(&f, image_size);
    put32(&f, KILN_PE_HEADERS_SIZE);
    put32(&f, 0);  // CheckSum -- not validated by the loader for normal exe launch
    put16(&f, 3);  // Subsystem = WINDOWS_CUI (console)
    put16(&f, 0);  // DllCharacteristics -- deliberately no DYNAMIC_BASE: fixed-base, no relocation table
    put64(&f, 0x100000); put64(&f, 0x1000); // stack reserve/commit
    put64(&f, 0x100000); put64(&f, 0x1000); // heap reserve/commit
    put32(&f, 0);  // LoaderFlags
    put32(&f, 16); // NumberOfRvaAndSizes
    for (int i = 0; i < 16; i++) {
        if (i == 1) { put32(&f, KILN_PE_IMPORT_DIR_ADDR - KILN_PE_IMAGE_BASE); put32(&f, KILN_PE_IMPORT_DIR_SIZE); }
        else if (i == 12) { put32(&f, KILN_PE_IAT_ADDR - KILN_PE_IMAGE_BASE); put32(&f, KILN_PE_IAT_SIZE); }
        else { put32(&f, 0); put32(&f, 0); }
    }

    // ---- IMAGE_SECTION_HEADER (one RWX section covering everything) ----
    const char *sec_name = ".text";
    for (int i = 0; i < 8; i++) put8(&f, (uint8_t)(i < (int)strlen(sec_name) ? sec_name[i] : 0));
    put32(&f, section_virtual_size);
    put32(&f, KILN_PE_SECTION_RVA);
    put32(&f, section_raw_size);
    put32(&f, KILN_PE_HEADERS_SIZE); // PointerToRawData
    put32(&f, 0); put32(&f, 0);      // relocations/linenumbers pointers
    put16(&f, 0); put16(&f, 0);      // relocation/linenumber counts
    put32(&f, 0x20u | 0x20000000u | 0x40000000u | 0x80000000u); // CODE|EXECUTE|READ|WRITE

    while (f.count < (int)KILN_PE_HEADERS_SIZE) put8(&f, 0);

    // ---- section content ----
    put_zeros(&f, KILN_PE_GLOBALS_SIZE);

    // Import Directory Table: one descriptor for kernel32.dll + terminator.
    // dll_name_rva was already computed above (the whole tail layout is
    // known before any bytes are emitted), so this is a direct write, no
    // patch-later step needed.
    put32(&f, KILN_PE_ILT_ADDR - KILN_PE_IMAGE_BASE); // OriginalFirstThunk -> ILT
    put32(&f, 0); put32(&f, 0);                        // TimeDateStamp, ForwarderChain
    put32(&f, dll_name_rva);                            // Name -> DLL name string
    put32(&f, KILN_PE_IAT_ADDR - KILN_PE_IMAGE_BASE);  // FirstThunk -> IAT
    put_zeros(&f, 20); // terminator descriptor

    // ILT and IAT: identical initial content, each entry an RVA to this
    // import's Hint/Name entry (bit63=0 selects import-by-name).
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < PE_IMPORT_COUNT_; i++) put64(&f, (uint64_t)hint_name_offsets[i]);
        put64(&f, 0); // terminator
    }

    for (int i = 0; i < machine_code->count; i++) put8(&f, machine_code->code[i]);

    for (int i = 0; i < PE_IMPORT_COUNT_; i++) {
        uint32_t entry_size = hint_name_entry_size(pe_import_names[i]);
        put16(&f, 0); // hint (0 = "don't trust it, look up by name")
        put_str(&f, pe_import_names[i]);
        if (entry_size > 2 + strlen(pe_import_names[i]) + 1) put8(&f, 0); // even-length pad
    }
    put_str(&f, "kernel32.dll");

    while (f.count < (int)(KILN_PE_HEADERS_SIZE + section_raw_size)) put8(&f, 0);

    FILE *out = fopen(path, "wb");
    if (!out) { fprintf(stderr, "kiln: could not open '%s' for writing\n", path); free(f.code); return 1; }
    size_t written = fwrite(f.code, 1, (size_t)f.count, out);
    fclose(out);
    free(f.code);
    if (written != (size_t)f.count) { fprintf(stderr, "kiln: short write to '%s'\n", path); return 1; }
    return 0;
}
