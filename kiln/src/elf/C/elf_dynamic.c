#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "elf/H/elf_dynamic.h"

// Hand-written dynamically-linked ELF64 executable: PT_INTERP so the
// kernel hands off to the REAL system dynamic linker, PT_DYNAMIC with
// DT_NEEDED=libkilnrt.so, a SysV-hash-format .hash + .dynsym + .dynstr
// (no section headers, matching this project's "tiny ELF" philosophy --
// ld.so only ever reads PT_DYNAMIC and what it points at, never section
// headers), and a GOT filled in by ld.so's own relocation processing
// before this program's own code ever runs (eager/non-lazy binding via
// R_X86_64_GLOB_DAT relocations -- simpler and just as correct as a lazy
// PLT stub sequence for kiln's purposes, and far less to get subtly
// wrong). See elf_dynamic.h for why .dynstr specifically is written
// AFTER the code rather than at a fixed offset.

static const char *const runtime_names[] = {
#define RUNTIME_EXPORT(name, str) str,
#include "codegen/H/emit/runtime_exports.def"
#undef RUNTIME_EXPORT
};

uint64_t elf_dynamic_got_addr(RuntimeImport which) {
    return KILN_DYN_GOT_ADDR + 8ULL * (uint64_t)which;
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

// Standard SysV ELF hash (elf_hash), the same algorithm every real
// DT_HASH-format .hash table in the wild uses -- ld.so implements this
// exact function to look symbols up, so this has to match it precisely.
static uint32_t elf_hash(const char *name) {
    uint32_t h = 0, g;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        h = (h << 4) + *p;
        g = h & 0xf0000000u;
        if (g) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

int elf_write_dynamic_executable(const char *path, const CodeBuf *machine_code) {
    // ---- pre-compute .dynstr layout (written after the code, but its
    // internal offsets are needed now for dynsym/dynamic/rela) ----
    uint32_t dynstr_off[RUNTIME_IMPORT_COUNT_];
    uint32_t cursor = 1; // leading empty string at offset 0
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) { dynstr_off[i] = cursor; cursor += (uint32_t)strlen(runtime_names[i]) + 1; }
    uint32_t soname_off = cursor; cursor += (uint32_t)strlen(KILN_LIBKILNRT_SONAME) + 1;
    uint32_t rpath_off = cursor; cursor += (uint32_t)strlen(KILN_RPATH) + 1;
    uint32_t dynstr_size = cursor;

    uint64_t code_start_file_off = KILN_DYN_CODE_START_OFFSET;
    uint64_t file_total = code_start_file_off + (uint64_t)machine_code->count + dynstr_size;
    uint64_t entry = KILN_LOAD_BASE + code_start_file_off;
    uint64_t dynstr_addr = KILN_LOAD_BASE + code_start_file_off + (uint64_t)machine_code->count;

    CodeBuf f;
    code_init(&f);

    // ---- Elf64_Ehdr ----
    put8(&f, 0x7f); put8(&f, 'E'); put8(&f, 'L'); put8(&f, 'F');
    put8(&f, 2); put8(&f, 1); put8(&f, 1); put8(&f, 0);
    put_zeros(&f, 8);
    put16(&f, 2);              // e_type = ET_EXEC
    put16(&f, 62);              // e_machine = EM_X86_64
    put32(&f, 1);
    put64(&f, entry);
    put64(&f, KILN_EHDR_SIZE);
    put64(&f, 0);
    put32(&f, 0);
    put16(&f, KILN_EHDR_SIZE);
    put16(&f, KILN_PHDR_SIZE);
    put16(&f, 3); // e_phnum: LOAD, INTERP, DYNAMIC
    put16(&f, 0); put16(&f, 0); put16(&f, 0);

    // ---- Elf64_Phdr[0]: PT_LOAD, whole file, RWX (matches this
    // project's existing single-segment ELF philosophy) ----
    put32(&f, 1); put32(&f, 7);
    put64(&f, 0); put64(&f, KILN_LOAD_BASE); put64(&f, KILN_LOAD_BASE);
    put64(&f, file_total); put64(&f, file_total); put64(&f, 0x1000);

    // ---- Elf64_Phdr[1]: PT_INTERP ----
    uint64_t interp_file_off = KILN_EHDR_SIZE + 3u * KILN_PHDR_SIZE;
    put32(&f, 3); put32(&f, 4); // PT_INTERP, PF_R
    put64(&f, interp_file_off); put64(&f, KILN_DYN_INTERP_ADDR); put64(&f, KILN_DYN_INTERP_ADDR);
    put64(&f, KILN_DYN_INTERP_SIZE); put64(&f, KILN_DYN_INTERP_SIZE); put64(&f, 1);

    // ---- Elf64_Phdr[2]: PT_DYNAMIC ----
    uint64_t dyn_file_off = KILN_DYN_TABLE_ADDR - KILN_LOAD_BASE;
    put32(&f, 2); put32(&f, 6); // PT_DYNAMIC, PF_R|PF_W
    put64(&f, dyn_file_off); put64(&f, KILN_DYN_TABLE_ADDR); put64(&f, KILN_DYN_TABLE_ADDR);
    put64(&f, KILN_DYN_TABLE_SIZE); put64(&f, KILN_DYN_TABLE_SIZE); put64(&f, 8);

    // ---- .interp ----
    put_str(&f, KILN_INTERP_STR);

    // ---- .hash (SysV DT_HASH format: nbucket, nchain, bucket[], chain[]) ----
    uint32_t buckets[KILN_DYN_NBUCKET]; for (unsigned i = 0; i < KILN_DYN_NBUCKET; i++) buckets[i] = 0;
    uint32_t chains[KILN_DYN_NSYMS]; for (unsigned i = 0; i < KILN_DYN_NSYMS; i++) chains[i] = 0;
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) {
        uint32_t sym_index = (uint32_t)i + 1;
        uint32_t b = elf_hash(runtime_names[i]) % KILN_DYN_NBUCKET;
        chains[sym_index] = buckets[b];
        buckets[b] = sym_index;
    }
    put32(&f, KILN_DYN_NBUCKET); put32(&f, KILN_DYN_NSYMS);
    for (unsigned i = 0; i < KILN_DYN_NBUCKET; i++) put32(&f, buckets[i]);
    for (unsigned i = 0; i < KILN_DYN_NSYMS; i++) put32(&f, chains[i]);

    // ---- .dynsym: null entry + one UNDEFINED entry per imported function
    // (st_shndx=SHN_UNDEF=0 -- "defined elsewhere", exactly what tells
    // ld.so to go resolve it against a NEEDED library rather than treat
    // it as locally defined) ----
    put32(&f, 0); put8(&f, 0); put8(&f, 0); put16(&f, 0); put64(&f, 0); put64(&f, 0); // null symbol
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) {
        put32(&f, dynstr_off[i]);          // st_name
        put8(&f, (1 << 4) | 2);            // st_info: STB_GLOBAL | STT_FUNC
        put8(&f, 0);                        // st_other
        put16(&f, 0);                       // st_shndx = SHN_UNDEF
        put64(&f, 0);                       // st_value
        put64(&f, 0);                       // st_size
    }

    // ---- GOT: zeroed, ld.so fills each slot via the RELA entries below ----
    put_zeros(&f, KILN_DYN_GOT_SIZE);

    // ---- .dynamic ----
    #define DT(tag, val) do { put64(&f, (uint64_t)(tag)); put64(&f, (uint64_t)(val)); } while (0)
    DT(1, soname_off);                 // DT_NEEDED (index into .dynstr)
    DT(15, rpath_off);                 // DT_RPATH
    DT(4, KILN_DYN_HASH_ADDR);         // DT_HASH
    DT(5, dynstr_addr);                // DT_STRTAB (after the code -- depends on its length)
    DT(6, KILN_DYN_SYMTAB_ADDR);       // DT_SYMTAB
    DT(10, dynstr_size);               // DT_STRSZ
    DT(11, KILN_DYN_SYMENT_SIZE);      // DT_SYMENT
    DT(7, KILN_DYN_RELA_ADDR);         // DT_RELA
    DT(8, KILN_DYN_RELA_SIZE);         // DT_RELASZ
    DT(9, KILN_DYN_RELAENT_SIZE);      // DT_RELAENT
    DT(0, 0);                          // DT_NULL
    #undef DT

    // ---- .rela.dyn: one R_X86_64_GLOB_DAT per GOT slot ----
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) {
        uint64_t sym_index = (uint64_t)i + 1;
        uint64_t r_info = (sym_index << 32) | 6ULL; // R_X86_64_GLOB_DAT = 6
        put64(&f, KILN_DYN_GOT_ADDR + 8ULL * (uint64_t)i); // r_offset
        put64(&f, r_info);
        put64(&f, 0);                                       // r_addend
    }

    put_zeros(&f, KILN_DYN_GLOBALS_SIZE);

    for (int i = 0; i < machine_code->count; i++) put8(&f, machine_code->code[i]);

    // ---- .dynstr tail ----
    put8(&f, 0); // offset 0 = empty string
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) put_str(&f, runtime_names[i]);
    put_str(&f, KILN_LIBKILNRT_SONAME);
    put_str(&f, KILN_RPATH);

    FILE *out = fopen(path, "wb");
    if (!out) { fprintf(stderr, "kiln: could not open '%s' for writing\n", path); free(f.code); return 1; }
    size_t written = fwrite(f.code, 1, (size_t)f.count, out);
    fclose(out);
    free(f.code);
    if (written != (size_t)f.count) { fprintf(stderr, "kiln: short write to '%s'\n", path); return 1; }
    if (chmod(path, 0755) != 0) { fprintf(stderr, "kiln: could not chmod '%s' executable\n", path); return 1; }
    return 0;
}
