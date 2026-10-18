#ifndef KILN_NUMBER_FORMAT_H
#define KILN_NUMBER_FORMAT_H
#include "codegen/H/emit/emit.h"

// Ash's one number-to-text format (ashvm's vm_format_number matches it):
//   - NaN / inf / -inf  -> "nan" / "inf" / "-inf"
//   - |x| < 1e15 and |x| >= 1e-6 (or 0): plain decimal, at most 6 fraction
//     digits (rounded), trailing zeros dropped, so 3.0 -> "3", 0.5 -> "0.5"
//   - otherwise scientific: mantissa as above in [1,10), then 'e', a sign and
//     at least two exponent digits ("1e+21", "1.5e-07")
//
// In:  RAX = the double's raw bits.
// Out: RCX = first byte, RDX = one past the last byte of the text, which lives
//      in a 64-byte scratch region on the stack. RSP has been lowered by
//      NUMBER_FORMAT_SCRATCH; the caller consumes the text, then does
//      `add rsp, NUMBER_FORMAT_SCRATCH`.
// Clobbers RAX, RBX, RCX, RDX, RSI, RDI, XMM0-XMM3.
#define NUMBER_FORMAT_SCRATCH 64
void number_format_emit(CodeBuf *code);

#endif
