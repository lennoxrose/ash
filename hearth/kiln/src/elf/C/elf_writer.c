#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "elf/H/elf_writer.h"

static void put8(CodeBuf *f, uint8_t v) {
    if (f->count >= f->capacity) {
        f->capacity = f->capacity == 0 ? 256 : f->capacity * 2;
        f->code = realloc(f->code, f->capacity);
    }
    f->code[f->count++] = v;
}
static void put16(CodeBuf *f, uint16_t v) { for (int i = 0; i < 2; i++) put8(f, (uint8_t)(v >> (8 * i))); }
static void put32(CodeBuf *f, uint32_t v) { for (int i = 0; i < 4; i++) put8(f, (uint8_t)(v >> (8 * i))); }
static void put64(CodeBuf *f, uint64_t v) { for (int i = 0; i < 8; i++) put8(f, (uint8_t)(v >> (8 * i))); }

int elf_write_executable(const char *path, const CodeBuf *machine_code) {
    CodeBuf file;
    code_init(&file);

    uint64_t total_size = KILN_CODE_START_OFFSET + (uint64_t)machine_code->count;
    uint64_t entry = KILN_LOAD_BASE + KILN_CODE_START_OFFSET;

    // ---- Elf64_Ehdr ----
    put8(&file, 0x7f); put8(&file, 'E'); put8(&file, 'L'); put8(&file, 'F');
    put8(&file, 2);  // ELFCLASS64
    put8(&file, 1);  // ELFDATA2LSB (little-endian)
    put8(&file, 1);  // EV_CURRENT
    put8(&file, 0);  // ELFOSABI_SYSV
    for (int i = 0; i < 8; i++) put8(&file, 0); // ABI version + padding, e_ident[9..15]

    put16(&file, 2);                // e_type = ET_EXEC
    put16(&file, 62);               // e_machine = EM_X86_64
    put32(&file, 1);                // e_version = EV_CURRENT
    put64(&file, entry);            // e_entry
    put64(&file, KILN_EHDR_SIZE);   // e_phoff (program header follows Ehdr immediately)
    put64(&file, 0);                // e_shoff (no section headers)
    put32(&file, 0);                // e_flags
    put16(&file, KILN_EHDR_SIZE);   // e_ehsize
    put16(&file, KILN_PHDR_SIZE);   // e_phentsize
    put16(&file, 1);                // e_phnum
    put16(&file, 0);                // e_shentsize
    put16(&file, 0);                // e_shnum
    put16(&file, 0);                // e_shstrndx

    // ---- Elf64_Phdr: one PT_LOAD segment covering the whole file ----
    put32(&file, 1);           // p_type = PT_LOAD
    // p_flags = PF_R | PF_W | PF_X. Milestone 1-4 only needed R|X (code
    // never wrote into its own segment); milestone 5's heap bump pointer
    // (see elf_writer.h / codegen/C/runtime/heap.c) is a global living inside this
    // same segment, so it needs to be writable too. A real compiler would
    // split .text (R|X) from .data (R|W) into separate segments -- kiln
    // doesn't do that yet, so the whole thing is RWX for now.
    put32(&file, 7);
    put64(&file, 0);                    // p_offset
    put64(&file, KILN_LOAD_BASE);       // p_vaddr
    put64(&file, KILN_LOAD_BASE);       // p_paddr
    put64(&file, total_size);           // p_filesz
    put64(&file, total_size);           // p_memsz
    put64(&file, 0x1000);               // p_align

    // ---- globals (heap_ptr etc.) -- zero-initialized ----
    for (int i = 0; i < KILN_GLOBALS_SIZE; i++) put8(&file, 0);

    // ---- code ----
    for (int i = 0; i < machine_code->count; i++) put8(&file, machine_code->code[i]);

    FILE *out = fopen(path, "wb");
    if (!out) { fprintf(stderr, "kiln: could not open '%s' for writing\n", path); free(file.code); return 1; }
    size_t written = fwrite(file.code, 1, (size_t)file.count, out);
    fclose(out);
    free(file.code);
    if (written != (size_t)file.count) {
        fprintf(stderr, "kiln: short write to '%s'\n", path);
        return 1;
    }

    if (chmod(path, 0755) != 0) {
        fprintf(stderr, "kiln: could not chmod '%s' executable\n", path);
        return 1;
    }
    return 0;
}
