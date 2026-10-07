#ifndef KILN_EMIT_H
#define KILN_EMIT_H
#include <stdint.h>

// Raw growable machine-code byte buffer, filled in directly (no assembler,
// no external tool) -- same "hand-encode the bytes yourself" spirit as
// ashvm/benchmarks/native_jit.c, generalized into a reusable module.
typedef struct {
    uint8_t *code;
    int count;
    int capacity;
} CodeBuf;

void code_init(CodeBuf *buf);

// x86-64 general-purpose register numbers (SysV encoding, low 8 only --
// kiln milestone 1 never needs r8-r15, so no REX.R/X/B is ever required).
typedef enum {
    REG_RAX = 0, REG_RCX = 1, REG_RDX = 2, REG_RBX = 3,
    REG_RSP = 4, REG_RBP = 5, REG_RSI = 6, REG_RDI = 7
} Reg;

typedef enum {
    COND_E  = 0x4, // je  / jz
    COND_NE = 0x5, // jne / jnz
    COND_LT = 0xC, // jl  (signed <)  -- integer cmp only
    COND_GE = 0xD, // jge (signed >=) -- integer cmp only
    COND_LE = 0xE, // jle (signed <=) -- integer cmp only
    COND_GT = 0xF, // jg  (signed >)  -- integer cmp only
    COND_B  = 0x2, // jb  (CF=1)      -- after ucomisd: a < b
    COND_AE = 0x3, // jae (CF=0)      -- after ucomisd: a >= b
    COND_BE = 0x6, // jbe (CF=1|ZF=1) -- after ucomisd: a <= b
    COND_A  = 0x7  // ja  (CF=0&ZF=0) -- after ucomisd: a > b
} Cond;

// All instructions below operate on full 64-bit registers unless noted.
void emit_mov_reg_imm64(CodeBuf *buf, Reg dst, uint64_t imm);
void emit_mov_reg_reg(CodeBuf *buf, Reg dst, Reg src);
void emit_add_reg_reg(CodeBuf *buf, Reg dst, Reg src);
void emit_add_reg_imm8(CodeBuf *buf, Reg reg, int8_t imm);
void emit_add_reg_imm32(CodeBuf *buf, Reg reg, int32_t imm);
void emit_sub_reg_reg(CodeBuf *buf, Reg dst, Reg src);
void emit_sub_reg_imm8(CodeBuf *buf, Reg reg, int8_t imm);
void emit_sub_reg_imm32(CodeBuf *buf, Reg reg, int32_t imm);
void emit_imul_reg_reg(CodeBuf *buf, Reg dst, Reg src);
void emit_idiv_reg(CodeBuf *buf, Reg reg);
void emit_neg_reg(CodeBuf *buf, Reg reg);
void emit_dec_reg(CodeBuf *buf, Reg reg);
void emit_cqo(CodeBuf *buf);
void emit_cmp_reg_imm32(CodeBuf *buf, Reg reg, int32_t imm);
void emit_cmp_reg_reg(CodeBuf *buf, Reg a, Reg b);

// Bitwise operators (missing.md): operands are truncated to int64 first
// (same cvttsd2si conversion floor()/the array-index paths already use),
// matching this project's existing "assume NUMBER, no runtime type
// error" scope limit for non-tag-aware operators.
void emit_and_reg_reg(CodeBuf *buf, Reg dst, Reg src);
void emit_or_reg_reg(CodeBuf *buf, Reg dst, Reg src);
void emit_xor_reg_reg(CodeBuf *buf, Reg dst, Reg src);
void emit_not_reg(CodeBuf *buf, Reg reg); // unary ~
// Shift count comes from CL (the only register-specified form x86-64
// offers for a variable shift amount) -- reg must not be RCX itself.
void emit_shl_reg_cl(CodeBuf *buf, Reg reg);
void emit_sar_reg_cl(CodeBuf *buf, Reg reg); // arithmetic (sign-preserving) shift right
void emit_sar_reg_by1(CodeBuf *buf, Reg reg); // shift right by exactly 1 (no CL needed) -- the exponent-by-squaring loop's n >>= 1
void emit_push_reg(CodeBuf *buf, Reg reg);
void emit_pop_reg(CodeBuf *buf, Reg reg);

// mov [base+disp32], src / mov dst, [base+disp32] -- mod=10 disp32
// addressing, safe for RBP (unlike the mod=00 forms emit_store_byte_* use,
// mod=10 sidesteps the "mod=00,rm=101 means RIP-relative" special case).
// disp32 (not disp8) because variable slots are 8 bytes apart and a
// 64-slot table needs offsets up to -512, past disp8's -128..127 range.
void emit_store_mem_disp32(CodeBuf *buf, Reg base, int32_t disp, Reg src);
void emit_load_mem_disp32(CodeBuf *buf, Reg dst, Reg base, int32_t disp);

// Byte (8-bit) memory stores through a register-indirect address [reg] --
// `reg` must not be RSP/RBP (both need a SIB/disp encoding this module
// deliberately doesn't support; callers route byte-buffer pointers through
// RCX instead, see codegen/print_int.c).
void emit_store_byte_imm(CodeBuf *buf, Reg base, uint8_t imm);
void emit_store_byte_reg(CodeBuf *buf, Reg base, Reg src); // src must be RAX/RCX/RDX/RBX (low byte AL/CL/DL/BL)
void emit_load_byte_reg(CodeBuf *buf, Reg dst, Reg base);  // dst = zero-extended byte from [base] (MOVZX)

void emit_syscall(CodeBuf *buf);

// Returns the byte offset of the rel32 placeholder to pass to
// emit_patch_jump() once the jump target is known (forward jump).
int emit_jcc_rel32(CodeBuf *buf, Cond cc);
int emit_jmp_rel32(CodeBuf *buf);
void emit_patch_jump(CodeBuf *buf, int patch_offset);

// Backward jump to an already-known target (loop back-edges), computed
// immediately -- no patching needed.
void emit_jmp_back(CodeBuf *buf, int target_offset);
void emit_jcc_back(CodeBuf *buf, Cond cc, int target_offset);

// Backward-only relative CALL (kiln only calls functions declared earlier
// in the source, same single-pass limitation ashvm's compiler has -- so
// the target's code offset is always already known, no patching needed).
void emit_call_back(CodeBuf *buf, int target_offset);
void emit_ret(CodeBuf *buf);

// CALL r/m64 (opcode FF /2) -- indirect call through a register holding
// an absolute runtime address. Needed for calling through a closure
// object whose target isn't known until the value is loaded at runtime,
// unlike emit_call_back's compile-time-known backward-only calls.
void emit_call_indirect(CodeBuf *buf, Reg target);

// JMP r/m64 (opcode FF /4) -- indirect jump through a register holding an
// absolute runtime address, no return address pushed (unlike
// emit_call_indirect). Used by try/catch's raise routine to resume at a
// catch block once RSP/RBP have already been unwound to the handler's
// saved values -- this is a resumption, not a call, so nothing should be
// pushed.
void emit_jmp_indirect(CodeBuf *buf, Reg target);

// "Emit now, patch later" for an 8-byte absolute-address immediate
// (mov reg, imm64), generalizing emit_jmp_rel32/emit_jcc_rel32's pattern
// to something other than a rel32 jump target: a lambda literal's
// absolute code address isn't known until AFTER its body compiles (which
// happens after the closure-creation code that needs to embed it), so the
// mov is emitted with a zero placeholder and the returned offset is
// passed to emit_patch_imm64() once the real address is known.
int emit_mov_reg_imm64_patchable(CodeBuf *buf, Reg dst);
void emit_patch_imm64(CodeBuf *buf, int patch_offset, uint64_t value);

// LEA reg, [rip+disp32] -- Plan B: the one primitive kiln's runtime code
// needs to reference its OWN embedded data once it can be loaded at a
// position ld.so chooses (libkilnrt.so), unlike every other address in
// this project which is computed as a KILN_LOAD_BASE-relative compile-time
// constant (only valid for kiln's always-fixed-base EXECUTABLES). Same
// "emit now, patch later" shape as emit_jmp_rel32: returns the patch
// offset, pass it to emit_patch_jump() once the target offset is known
// (the disp32 is relative to the byte right after itself, i.e. RIP at
// that point -- identical math to a rel32 jump, just a different opcode).
int emit_lea_rip_disp32(CodeBuf *buf, Reg dst);
void emit_lea_rip_back(CodeBuf *buf, Reg dst, int target_offset);

// CALL [addr] via 32-bit absolute-displacement addressing -- touches no
// register at all. `addr` must fit in 31 bits (true for every kiln
// fixed-base address). See emit_mem.c for the encoding rationale.
void emit_call_abs32(CodeBuf *buf, uint32_t addr);

#endif
