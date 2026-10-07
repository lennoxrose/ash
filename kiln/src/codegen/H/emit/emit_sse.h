#ifndef KILN_EMIT_SSE_H
#define KILN_EMIT_SSE_H
#include "codegen/emit.h"

// Scalar double-precision (SSE2) primitives -- milestone 4 replaces
// kiln's integer-only numbers with real doubles throughout, matching
// ashvm's VMValue.number (always double). Kiln milestone 1-3 never
// needed these; everything user-visible now flows through XMM registers
// instead of GP registers. GP registers are still used for internal
// plumbing that isn't an ash-visible value (loop counters in the print
// routine, syscall arguments, frame pointers).
typedef enum { XMM0 = 0, XMM1 = 1, XMM2 = 2, XMM3 = 3 } XReg;

void emit_movq_xmm_from_reg(CodeBuf *buf, XReg dst, Reg src); // MOVQ xmm, r64 -- loads a GP register's raw 64 bits as a double's bit pattern (used for literals: the literal's IEEE-754 bits are computed once in kiln itself, at compile time)
void emit_movq_reg_from_xmm(CodeBuf *buf, Reg dst, XReg src); // MOVQ r64, xmm -- the reverse: extracts a computed double's raw bits into a GP register, milestone 5's generic (tag, payload) values are always GP-transported (see codegen/value.h), so an arithmetic result has to come back out of XMM before it can be pushed
void emit_movsd_xmm_xmm(CodeBuf *buf, XReg dst, XReg src);
void emit_movsd_load_disp32(CodeBuf *buf, XReg dst, Reg base, int32_t disp);
void emit_movsd_store_disp32(CodeBuf *buf, Reg base, int32_t disp, XReg src);
void emit_pxor_xmm_xmm(CodeBuf *buf, XReg reg); // zeroes a register (reg,reg) -- also used as "load +0.0"

void emit_addsd(CodeBuf *buf, XReg dst, XReg src);
void emit_subsd(CodeBuf *buf, XReg dst, XReg src);
void emit_mulsd(CodeBuf *buf, XReg dst, XReg src);
void emit_divsd(CodeBuf *buf, XReg dst, XReg src);
void emit_sqrtsd(CodeBuf *buf, XReg dst, XReg src);
void emit_andpd(CodeBuf *buf, XReg dst, XReg src); // milestone 10's abs(): AND against a sign-bit-clearing mask

void emit_cvttsd2si(CodeBuf *buf, Reg dst, XReg src); // truncate double -> int64
void emit_cvtsi2sd(CodeBuf *buf, XReg dst, Reg src);  // int64 -> double

// Sets flags the same way an unsigned integer compare would (CF/ZF, not
// SF/OF) -- callers must use COND_B/COND_BE/COND_A/COND_AE/COND_E/COND_NE
// after this, never the signed COND_LT/LE/GT/GE from emit.h.
void emit_ucomisd(CodeBuf *buf, XReg a, XReg b);

// x86-64 has no hardware PUSH/POP for XMM registers -- these hand-build
// the equivalent (sub rsp,8 + movsd [rsp] / movsd [rsp] + add rsp,8) so
// the same stack-machine codegen model expr.c/expr_bool.c already use for
// GP registers works unchanged for doubles. The fixed SIB byte (0x24)
// these use is the standard encoding for "[rsp]" with no displacement --
// RSP as a base always needs a SIB byte, unlike every other register.
void emit_push_xmm(CodeBuf *buf, XReg src);
void emit_pop_xmm(CodeBuf *buf, XReg dst);

#endif
