#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "elf/elf_so_writer.h"
#include "elf/elf_dynamic.h"
#include "codegen/heap.h"
#include "codegen/bytes.h"
#include "codegen/string_alloc.h"
#include "codegen/print_int.h"
#include "codegen/errors.h"
#include "target.h"

static const char *const export_names[] = {
#define RUNTIME_EXPORT(name, str) str,
#include "codegen/runtime_exports.def"
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

int elf_write_shared_runtime(const char *path) {
    // ---- compile the runtime itself, position-independent, into its own
    // fresh CodeBuf. Link mode STAYS shared (main.c sets it before
    // calling this) so codegen/layout.c resolves globals addresses using
    // the shared-mode executable layout -- but kiln_set_compiling_so(1)
    // makes each routine's OWN call sites (e.g. string_alloc_prefixed
    // calling heap_emit_alloc) resolve as ordinary same-buffer relative
    // calls instead of GOT calls back out to this very library. These
    // are two different questions (see target.h's comment) that a first
    // pass at this conflated into one flag, and testing caught it: doing
    // so ALSO silently broke the globals address computation. ----
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

    // ---- lay out the .so file: [ehdr][phdr LOAD][phdr DYNAMIC]
    // [hash][dynsym][dynstr][dynamic][code], all offsets/addresses
    // relative to 0 (ET_DYN: ld.so adds its chosen load bias to
    // everything) ----
    unsigned nsyms = (unsigned)RUNTIME_IMPORT_COUNT_ + 1u;
    unsigned nbucket = nsyms;

    uint32_t dynstr_off[RUNTIME_IMPORT_COUNT_];
    uint32_t cursor = 1; // leading empty string
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) { dynstr_off[i] = cursor; cursor += (uint32_t)strlen(export_names[i]) + 1; }
    uint32_t soname_off = cursor; cursor += (uint32_t)strlen(KILN_LIBKILNRT_SONAME) + 1;
    uint32_t dynstr_size = cursor;

    const uint32_t ehdr_size = KILN_EHDR_SIZE, phdr_size = KILN_PHDR_SIZE;
    uint32_t hash_off = ehdr_size + 2u * phdr_size;
    uint32_t hash_size = 8u + 4u * nbucket + 4u * nsyms;
    uint32_t dynsym_off = hash_off + hash_size;
    uint32_t dynsym_size = 24u * nsyms;
    uint32_t dynstr_file_off = dynsym_off + dynsym_size;
    uint32_t dynamic_off = dynstr_file_off + dynstr_size;
    uint32_t dynamic_nent = 7; // SONAME,HASH,STRTAB,SYMTAB,STRSZ,SYMENT,NULL
    uint32_t dynamic_size = dynamic_nent * 16u;
    uint32_t code_off = dynamic_off + dynamic_size;
    uint32_t total_size = code_off + (uint32_t)runtime.count;

    CodeBuf f;
    code_init(&f);

    put8(&f, 0x7f); put8(&f, 'E'); put8(&f, 'L'); put8(&f, 'F');
    put8(&f, 2); put8(&f, 1); put8(&f, 1); put8(&f, 0);
    put_zeros(&f, 8);
    put16(&f, 3); // e_type = ET_DYN
    put16(&f, 62);
    put32(&f, 1);
    put64(&f, 0); // e_entry (none -- a library, not directly executed)
    put64(&f, ehdr_size);
    put64(&f, 0);
    put32(&f, 0);
    put16(&f, (uint16_t)ehdr_size);
    put16(&f, (uint16_t)phdr_size);
    put16(&f, 2); // LOAD, DYNAMIC
    put16(&f, 0); put16(&f, 0); put16(&f, 0);

    put32(&f, 1); put32(&f, 7); // PT_LOAD, RWX
    put64(&f, 0); put64(&f, 0); put64(&f, 0);
    put64(&f, total_size); put64(&f, total_size); put64(&f, 0x1000);

    put32(&f, 2); put32(&f, 6); // PT_DYNAMIC, PF_R|PF_W
    put64(&f, dynamic_off); put64(&f, dynamic_off); put64(&f, dynamic_off);
    put64(&f, dynamic_size); put64(&f, dynamic_size); put64(&f, 8);

    // .hash
    uint32_t *buckets = calloc(nbucket, sizeof(uint32_t));
    uint32_t *chains = calloc(nsyms, sizeof(uint32_t));
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) {
        uint32_t sym_index = (uint32_t)i + 1;
        uint32_t b = elf_hash(export_names[i]) % nbucket;
        chains[sym_index] = buckets[b];
        buckets[b] = sym_index;
    }
    put32(&f, nbucket); put32(&f, nsyms);
    for (unsigned i = 0; i < nbucket; i++) put32(&f, buckets[i]);
    for (unsigned i = 0; i < nsyms; i++) put32(&f, chains[i]);
    free(buckets); free(chains);

    // .dynsym: null + one DEFINED entry per export (st_value = its offset
    // within this image, st_shndx=1 -- a nonzero placeholder, since this
    // file has no section headers to index; ld.so never needs one).
    put32(&f, 0); put8(&f, 0); put8(&f, 0); put16(&f, 0); put64(&f, 0); put64(&f, 0);
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) {
        put32(&f, dynstr_off[i]);
        put8(&f, (1 << 4) | 2); // STB_GLOBAL | STT_FUNC
        put8(&f, 0);
        put16(&f, 1);
        put64(&f, (uint64_t)(code_off + (uint32_t)offsets[i]));
        put64(&f, 0);
    }

    // .dynstr
    put8(&f, 0);
    for (int i = 0; i < RUNTIME_IMPORT_COUNT_; i++) put_str(&f, export_names[i]);
    put_str(&f, KILN_LIBKILNRT_SONAME);

    // .dynamic
    #define DT(tag, val) do { put64(&f, (uint64_t)(tag)); put64(&f, (uint64_t)(val)); } while (0)
    DT(14, soname_off);      // DT_SONAME
    DT(4, hash_off);         // DT_HASH
    DT(5, dynstr_file_off);  // DT_STRTAB
    DT(6, dynsym_off);       // DT_SYMTAB
    DT(10, dynstr_size);     // DT_STRSZ
    DT(11, 24);              // DT_SYMENT
    DT(0, 0);                // DT_NULL
    #undef DT

    for (int i = 0; i < runtime.count; i++) put8(&f, runtime.code[i]);
    free(runtime.code);

    FILE *out = fopen(path, "wb");
    if (!out) { fprintf(stderr, "kiln: could not open '%s' for writing\n", path); free(f.code); return 1; }
    size_t written = fwrite(f.code, 1, (size_t)f.count, out);
    fclose(out);
    free(f.code);
    if (written != (size_t)f.count) { fprintf(stderr, "kiln: short write to '%s'\n", path); return 1; }
    if (chmod(path, 0755) != 0) { fprintf(stderr, "kiln: could not chmod '%s'\n", path); return 1; }
    return 0;
}
