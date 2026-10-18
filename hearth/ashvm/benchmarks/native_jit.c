// Hand-written x86-64 JIT for one fixed pattern:
//   sum = 0; i = 0; while (i < N) { sum += i; i++; }
// No VM, no bytecode dispatch, no interpreter — this mmaps a page,
// writes real machine code bytes into it, marks it executable,
// and calls it directly as a C function pointer.

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>

typedef long long (*fn_t)(void);

static void emit8(uint8_t *buf, int *pos, uint8_t b) {
    buf[(*pos)++] = b;
}
static void emit32(uint8_t *buf, int *pos, int32_t v) {
    memcpy(buf + *pos, &v, 4);
    *pos += 4;
}

int main(void) {
    long long N = 20000000; // 20 million iterations, same as the Ash benchmark

    void *mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { perror("mmap"); return 1; }
    uint8_t *code = (uint8_t *)mem;
    int pos = 0;

    // xor eax, eax      ; sum = 0
    emit8(code, &pos, 0x31); emit8(code, &pos, 0xC0);
    // xor ecx, ecx       ; i = 0
    emit8(code, &pos, 0x31); emit8(code, &pos, 0xC9);

    int loop_start = pos;
    // cmp rcx, N (imm32)
    emit8(code, &pos, 0x48); emit8(code, &pos, 0x81); emit8(code, &pos, 0xF9);
    emit32(code, &pos, (int32_t)N);

    // jge exit (rel32, patched below)
    emit8(code, &pos, 0x0F); emit8(code, &pos, 0x8D);
    int jge_operand = pos;
    emit32(code, &pos, 0); // placeholder

    // add rax, rcx       ; sum += i
    emit8(code, &pos, 0x48); emit8(code, &pos, 0x01); emit8(code, &pos, 0xC8);
    // add rcx, 1         ; i++
    emit8(code, &pos, 0x48); emit8(code, &pos, 0x83); emit8(code, &pos, 0xC1); emit8(code, &pos, 0x01);

    // jmp loop_start
    int jmp_pos = pos;
    emit8(code, &pos, 0xE9);
    int32_t jmp_rel = loop_start - (jmp_pos + 5);
    emit32(code, &pos, jmp_rel);

    int exit_pos = pos;
    // ret                ; sum already in rax (SysV ABI return register)
    emit8(code, &pos, 0xC3);

    // patch the jge target now that we know where 'exit' actually is
    int32_t jge_rel = exit_pos - (jge_operand + 4);
    memcpy(code + jge_operand, &jge_rel, 4);

    if (mprotect(mem, 4096, PROT_READ | PROT_EXEC) != 0) { perror("mprotect"); return 1; }

    fn_t fn = (fn_t)(void *)code;
    long long result = fn();
    printf("%lld\n", result);

    return 0;
}
