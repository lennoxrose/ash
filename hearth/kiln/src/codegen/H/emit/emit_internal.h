#ifndef KILN_EMIT_INTERNAL_H
#define KILN_EMIT_INTERNAL_H
#include "codegen/H/emit/emit.h"

// Shared byte-buffer primitives and ModRM builders, used by both emit.c
// (register arithmetic) and emit_mem.c (memory/stack/jump instructions).
// Not part of the public API (emit.h).
#define REX_W 0x48

void emit_byte(CodeBuf *buf, uint8_t b);
void emit_i32(CodeBuf *buf, int32_t v);
void emit_u64(CodeBuf *buf, uint64_t v);

uint8_t modrm_reg(Reg reg_field, Reg rm_field);
uint8_t modrm_mem(Reg reg_field, Reg base);
uint8_t modrm_mem_disp32(Reg reg_field, Reg base);

#endif
