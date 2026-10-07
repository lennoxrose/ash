#include "elf/H/elf_dynamic_call.h"

void elf_dynamic_call(CodeBuf *code, RuntimeImport which) {
    emit_call_abs32(code, (uint32_t)elf_dynamic_got_addr(which));
}
