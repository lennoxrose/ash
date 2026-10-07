#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pe/H/pe_writer.h"

// --link=shared PE executable: same "tiny PE" single-RWX-section
// technique as pe_writer.c's static-mode writer, but importing from TWO
// DLLs (kernel32.dll for OS calls, libkilnrt.dll for the runtime -- see
// pe/H/pe_dll_writer.h for the library side).
//
// Section layout:
//   [globals][import dir: k32,rt,null][ILT k32][IAT k32][ILT rt][IAT rt]
//   [code][hint/name: k32's 9, rt's 5][kernel32.dll][libkilnrt.dll]

static const char *const pe_import_names[] = {
#define PE_IMPORT(name, str) str,
#include "pe/H/pe_imports.def"
#undef PE_IMPORT
};

static const char *const runtime_import_names[] = {
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
static uint32_t hint_name_entry_size(const char *name) {
    uint32_t n = 2 + (uint32_t)strlen(name) + 1;
    return (n % 2 == 0) ? n : n + 1;
}

int pe_write_dynamic_executable(const char *path, const CodeBuf *machine_code) {
    uint32_t code_rva = KILN_PE_SHARED_CODE_START_OFFSET;
    uint32_t tail_rva = code_rva + (uint32_t)machine_code->count;

    uint32_t k32_hint_off[PE_IMPORT_COUNT_];
    uint32_t cursor = tail_rva;
    for (int i = 0; i < PE_IMPORT_COUNT_; i++) { k32_hint_off[i] = cursor; cursor += hint_name_entry_size(pe_import_names[i]); }
    uint32_t k32_name_rva = cursor; cursor += (uint32_t)strlen("kernel32.dll") + 1;

    uint32_t rt_hint_off[RUNTIME_IMPORT_COUNT_];
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) { rt_hint_off[i] = cursor; cursor += hint_name_entry_size(runtime_import_names[i]); }
    uint32_t rt_name_rva = cursor; cursor += (uint32_t)strlen(KILN_LIBKILNRT_DLL_NAME) + 1;

    uint32_t section_virtual_size = cursor - KILN_PE_SECTION_RVA;
    uint32_t section_raw_size = align_up(section_virtual_size, KILN_PE_FILE_ALIGN);
    uint32_t image_size = align_up(KILN_PE_SECTION_RVA + section_virtual_size, KILN_PE_SECTION_ALIGN);

    CodeBuf f;
    code_init(&f);

    put8(&f, 'M'); put8(&f, 'Z');
    put_zeros(&f, 58);
    put32(&f, 64);

    put8(&f, 'P'); put8(&f, 'E'); put8(&f, 0); put8(&f, 0);
    put16(&f, 0x8664);
    put16(&f, 1);
    put32(&f, 0); put32(&f, 0); put32(&f, 0);
    put16(&f, 240);
    put16(&f, 0x0002 | 0x0020);

    put16(&f, 0x20B);
    put8(&f, 14); put8(&f, 0);
    put32(&f, section_raw_size);
    put32(&f, 0); put32(&f, 0);
    put32(&f, code_rva);
    put32(&f, KILN_PE_SECTION_RVA);
    put64(&f, KILN_PE_IMAGE_BASE);
    put32(&f, KILN_PE_SECTION_ALIGN);
    put32(&f, KILN_PE_FILE_ALIGN);
    put16(&f, 6); put16(&f, 0);
    put16(&f, 0); put16(&f, 0);
    put16(&f, 6); put16(&f, 0);
    put32(&f, 0);
    put32(&f, image_size);
    put32(&f, KILN_PE_HEADERS_SIZE);
    put32(&f, 0);
    put16(&f, 3);
    put16(&f, 0);
    put64(&f, 0x100000); put64(&f, 0x1000);
    put64(&f, 0x100000); put64(&f, 0x1000);
    put32(&f, 0);
    put32(&f, 16);
    for (int i = 0; i < 16; i++) {
        if (i == 1) { put32(&f, KILN_PE_IMPORT_DIR_ADDR - KILN_PE_IMAGE_BASE); put32(&f, KILN_PE_SHARED_IMPORT_DIR_SIZE); }
        else if (i == 12) { put32(&f, KILN_PE_SHARED_IAT_K32_ADDR - KILN_PE_IMAGE_BASE); put32(&f, KILN_PE_SHARED_IAT_K32_SIZE + KILN_PE_SHARED_IAT_RT_SIZE); }
        else { put32(&f, 0); put32(&f, 0); }
    }

    const char *sec_name = ".text";
    for (int i = 0; i < 8; i++) put8(&f, (uint8_t)(i < (int)strlen(sec_name) ? sec_name[i] : 0));
    put32(&f, section_virtual_size);
    put32(&f, KILN_PE_SECTION_RVA);
    put32(&f, section_raw_size);
    put32(&f, KILN_PE_HEADERS_SIZE);
    put32(&f, 0); put32(&f, 0);
    put16(&f, 0); put16(&f, 0);
    put32(&f, 0x20u | 0x20000000u | 0x40000000u | 0x80000000u);

    while (f.count < (int)KILN_PE_HEADERS_SIZE) put8(&f, 0);

    put_zeros(&f, KILN_PE_GLOBALS_SIZE);

    // Import Directory Table: kernel32.dll, libkilnrt.dll, null terminator
    put32(&f, KILN_PE_SHARED_ILT_K32_ADDR - KILN_PE_IMAGE_BASE);
    put32(&f, 0); put32(&f, 0);
    put32(&f, k32_name_rva);
    put32(&f, KILN_PE_SHARED_IAT_K32_ADDR - KILN_PE_IMAGE_BASE);

    put32(&f, KILN_PE_SHARED_ILT_RT_ADDR - KILN_PE_IMAGE_BASE);
    put32(&f, 0); put32(&f, 0);
    put32(&f, rt_name_rva);
    put32(&f, KILN_PE_SHARED_IAT_RT_ADDR - KILN_PE_IMAGE_BASE);

    put_zeros(&f, 20); // terminator descriptor

    // Both ILTs, then both IATs (see pe_writer.h: keeps the combined IAT
    // region contiguous for the optional header's single {RVA,Size} pair).
    for (int i = 0; i < PE_IMPORT_COUNT_; i++) put64(&f, (uint64_t)k32_hint_off[i]);
    put64(&f, 0);
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) put64(&f, (uint64_t)rt_hint_off[i]);
    put64(&f, 0);
    for (int i = 0; i < PE_IMPORT_COUNT_; i++) put64(&f, (uint64_t)k32_hint_off[i]);
    put64(&f, 0);
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) put64(&f, (uint64_t)rt_hint_off[i]);
    put64(&f, 0);

    for (int i = 0; i < machine_code->count; i++) put8(&f, machine_code->code[i]);

    for (int i = 0; i < PE_IMPORT_COUNT_; i++) {
        uint32_t entry_size = hint_name_entry_size(pe_import_names[i]);
        put16(&f, 0);
        put_str(&f, pe_import_names[i]);
        if (entry_size > 2 + strlen(pe_import_names[i]) + 1) put8(&f, 0);
    }
    put_str(&f, "kernel32.dll");
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) {
        uint32_t entry_size = hint_name_entry_size(runtime_import_names[i]);
        put16(&f, 0);
        put_str(&f, runtime_import_names[i]);
        if (entry_size > 2 + strlen(runtime_import_names[i]) + 1) put8(&f, 0);
    }
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
